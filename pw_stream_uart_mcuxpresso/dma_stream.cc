// Copyright 2024 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

/*
 * Copyright 2018 - 2022 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pw_stream_uart_mcuxpresso/dma_stream.h"

#include "pw_preprocessor/util.h"

namespace pw::stream {

// Deinitialize the DMA channels and USART.
void UartDmaStreamMcuxpresso::Deinit() {
  // We need to touch register space that can be shared
  // among several DMA peripherals, hence we need to access
  // it exclusively. We achieve exclusive access on non-SMP systems as
  // a side effect of acquiring the interrupt_lock_, since acquiring the
  // interrupt_lock_ disables interrupts on the current CPU, which means
  // we cannot get descheduled until we release the interrupt_lock_.
  interrupt_lock_.lock();
  DMA_DisableChannel(config_.dma_base, config_.tx_dma_ch);
  DMA_DisableChannel(config_.dma_base, config_.rx_dma_ch);
  interrupt_lock_.unlock();

  USART_Deinit(config_.usart_base);
}

UartDmaStreamMcuxpresso::~UartDmaStreamMcuxpresso() {
  if (!initialized_) {
    return;
  }
  Deinit();
}

// Initialize the USART and DMA channels based on the configuration
// specified during object creation.
Status UartDmaStreamMcuxpresso::Init(uint32_t srcclk) {
  if (srcclk == 0) {
    return Status::InvalidArgument();
  }
  if (config_.usart_base == nullptr) {
    return Status::InvalidArgument();
  }
  if (config_.baud_rate == 0) {
    return Status::InvalidArgument();
  }
  if (config_.dma_base == nullptr) {
    return Status::InvalidArgument();
  }

  usart_config_t defconfig_;
  USART_GetDefaultConfig(&defconfig_);

  defconfig_.baudRate_Bps = config_.baud_rate;
  defconfig_.parityMode = config_.parity;
  defconfig_.enableTx = true;
  defconfig_.enableRx = true;

  status_t status = USART_Init(config_.usart_base, &defconfig_, srcclk);
  if (status != kStatus_Success) {
    return Status::Internal();
  }

  // We need to touch register space that can be shared
  // among several DMA peripherals, hence we need to access
  // it exclusively. We achieve exclusive access on non-SMP systems as
  // a side effect of acquiring the interrupt_lock_, since acquiring the
  // interrupt_lock_ disables interrupts on the current CPU, which means
  // we cannot get descheduled until we release the interrupt_lock_.
  interrupt_lock_.lock();

  INPUTMUX_Init(INPUTMUX);
  // Enable DMA request.
  INPUTMUX_EnableSignal(
      INPUTMUX, config_.rx_input_mux_dmac_ch_request_en, true);
  INPUTMUX_EnableSignal(
      INPUTMUX, config_.tx_input_mux_dmac_ch_request_en, true);
  // Turnoff clock to inputmux to save power. Clock is only needed to make
  // changes.
  INPUTMUX_Deinit(INPUTMUX);

  DMA_EnableChannel(config_.dma_base, config_.tx_dma_ch);
  DMA_EnableChannel(config_.dma_base, config_.rx_dma_ch);

  DMA_CreateHandle(&tx_data_.dma_handle, config_.dma_base, config_.tx_dma_ch);
  DMA_CreateHandle(&rx_data_.dma_handle, config_.dma_base, config_.rx_dma_ch);

  interrupt_lock_.unlock();

  status = USART_TransferCreateHandleDMA(config_.usart_base,
                                         &uart_dma_handle_,
                                         TxRxCompletionCallback,
                                         this,
                                         &tx_data_.dma_handle,
                                         &rx_data_.dma_handle);

  if (status != kStatus_Success) {
    Deinit();
    return Status::Internal();
  }

  initialized_ = true;
  return OkStatus();
}

// DMA send buffer data
void UartDmaStreamMcuxpresso::TriggerWriteDma() {
  const uint8_t* tx_buffer =
      reinterpret_cast<const uint8_t*>(tx_data_.buffer.data());
  tx_data_.transfer.txData = &tx_buffer[tx_data_.tx_idx];
  if (tx_data_.tx_idx + kUsartDmaMaxTransferCount >
      tx_data_.buffer.size_bytes()) {
    // Completion callback will be called once this transfer completes.
    tx_data_.transfer.dataSize = tx_data_.buffer.size_bytes() - tx_data_.tx_idx;
  } else {
    tx_data_.transfer.dataSize = kUsartDmaMaxTransferCount;
  }

  USART_TransferSendDMA(
      config_.usart_base, &uart_dma_handle_, &tx_data_.transfer);
}

// Completion callback for TX and RX transactions
void UartDmaStreamMcuxpresso::TxRxCompletionCallback(USART_Type* base,
                                                     usart_dma_handle_t* state,
                                                     status_t status,
                                                     void* param) {
  UartDmaStreamMcuxpresso* stream =
      reinterpret_cast<UartDmaStreamMcuxpresso*>(param);

  if (status == kStatus_USART_TxIdle) {
    // Tx transfer
    UsartDmaTxData* tx_data = &stream->tx_data_;
    tx_data->tx_idx += tx_data->transfer.dataSize;
    if (tx_data->tx_idx == tx_data->buffer.size_bytes()) {
      // We have completed the send request, we must wake up the sender.
      tx_data->notification.release();
    } else {
      PW_CHECK_INT_LT(tx_data->tx_idx, tx_data->buffer.size_bytes());
      stream->TriggerWriteDma();
    }
  }
}

StatusWithSize UartDmaStreamMcuxpresso::DoRead(ByteSpan data) {
  size_t length = data.size();
  return StatusWithSize(length);
}

// Write data to USART using DMA transactions
//
// Note: Only one thread should be calling this function,
// otherwise DoWrite calls might fail due to contention for
// the USART TX channel.
Status UartDmaStreamMcuxpresso::DoWrite(ConstByteSpan data) {
  if (data.size() == 0) {
    return Status::InvalidArgument();
  }

  bool was_busy = tx_data_.busy.exchange(true);
  if (was_busy) {
    // Another thread is already transmitting data.
    return Status::FailedPrecondition();
  }

  tx_data_.buffer = data;
  tx_data_.tx_idx = 0;

  TriggerWriteDma();

  tx_data_.notification.acquire();

  tx_data_.busy.store(false);

  return OkStatus();
}

}  // namespace pw::stream
