// Copyright 2023 The Pigweed Authors
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

import { css } from "lit";

export const styles = css`
    :host {
        display: flex;
        align-items: center;
        justify-content: space-between;
        height: 3rem;
        background: #3c4043;
        gap: 2rem;
        padding: 0 1rem;
    }

    :host > * {
        display: flex;
    }

    button {
        color: white;
    }

    .button-toggle {
        background-color: rgb(45, 49, 52);
    }

    .field-menu {
        background-color: rgb(72, 70, 76);
        border-radius: 4px;
        padding: 0.5rem 0.75rem;
        margin: 0;
        position: absolute;
        right: 0;
        z-index: 2;
    }

    .field-menu-item {
        align-items: center;
        display: flex;
        height: 3rem;
        width: max-content;
    }

    .field-toggle {
        position: relative;
        border-radius: 1.5rem;
    }

    .input-container {
        width: 100%;
    }

    input {
        width: 100%;
        max-width: 20rem;
        height: 2rem;
        border-radius: 1.5rem;
        border: none;
        font-family: "Google Sans";
        padding: 0 1rem;
        color: rgba(255, 255, 255, 0.87);
        background: #2d3134;
        /* border: 1px solid rgba(255, 255, 255, 0.87); */
    }

    input[type=checkbox] {
        height: 1.125rem;
        width: 1.125rem;
    }

    label {
        font-color: var(--md-sys-color-on-surface-variant);
        padding-left: 0.75rem;

    p {
        flex: 1 0;
        white-space: nowrap;
    }
`;
