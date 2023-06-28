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

import { LogSource } from '../log-source';
import { LogEntry, SeverityLevel } from '../shared/interfaces';

export class MockLogSource extends LogSource {
    private intervalId: NodeJS.Timeout | null = null;

    constructor() {
        super();
    }

    start(): void {
        const getRandomInterval = () => {
            return Math.floor(Math.random() * (1000 - 50 + 1) + 50);
        };

        const readLogEntry = () => {
            const logEntry = this.readLogEntryFromHost();
            this.emitEvent('logEntry', logEntry);

            const nextInterval = getRandomInterval();
            setTimeout(readLogEntry, nextInterval);
        };

        readLogEntry();
    }

    stop(): void {
        if (this.intervalId) {
            clearInterval(this.intervalId);
            this.intervalId = null;
        }
    }

    readLogEntryFromHost(): LogEntry {
        // Emulate reading log data from a host device
        const sources = ['application', 'server', 'database', 'network'];
        const messages = [
            'Request processed successfully!',
            'An unexpected error occurred while performing the operation.',
            'Connection timed out. Please check your network settings.',
            'Invalid input detected. Please provide valid data.',
            'Database connection lost. Attempting to reconnect.',
            'User authentication failed. Invalid credentials provided.',
            'System reboot initiated. Please wait for the system to come back online.',
            'File not found. The requested file does not exist.',
            'Data corruption detected. Initiating recovery process.',
            'Network congestion detected. Traffic is high, please try again later.',
            'Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nullam condimentum auctor justo, sit amet condimentum nibh facilisis non. Quisque in quam a urna dignissim cursus. Suspendisse egestas nisl sed massa dictum dictum. In tincidunt arcu nec odio eleifend, vel pharetra justo iaculis. Vivamus quis tellus ac velit vehicula consequat. Nam eu felis sed risus hendrerit faucibus ac id lacus. Vestibulum tincidunt tellus in ex feugiat interdum. Nulla sit amet luctus neque. Mauris et aliquet nunc, vel finibus massa. Curabitur laoreet eleifend nibh eget luctus. Fusce sodales augue nec purus faucibus, vel tristique enim vehicula. Aenean eu magna eros. Fusce accumsan dignissim dui auctor scelerisque. Proin ultricies nunc vel tincidunt facilisis.',
        ];
        const timestamp: Date = new Date();
        const getRandomValue = (values: string[]) => {
            const randomIndex = Math.floor(Math.random() * values.length);
            return values[randomIndex];
        };
        const severity = getRandomValue(Object.keys(SeverityLevel));
        const logEntry = {
            severity: severity as SeverityLevel,
            timestamp: timestamp,
            fields: [
                { key: 'timestamp', value: timestamp.toISOString() },
                { key: 'severity', value: severity },
                { key: 'source', value: getRandomValue(sources) },
                { key: 'message', value: getRandomValue(messages) },
            ],
        };
        return logEntry;
    }
}