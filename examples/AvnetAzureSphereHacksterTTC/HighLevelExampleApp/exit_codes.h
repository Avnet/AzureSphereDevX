/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

/// <summary>
/// Exit codes for the application. Exit codes 1 - 149 
/// are reserved for application level exit codes.
/// </summary>
typedef enum {

    ExitCode_ConsumeEventLoopWifiMonitor = 1,
    ExitCode_ConsumeEventLoopReadSensors = 2,
    ExitCode_ConsumeEventLoopRestart     = 3,
    ExitCode_ConsumeEventButtonHandler   = 4,
    ExitCode_ReadButtonAError            = 5,
    ExitCode_ReadButtonBError            = 6,   
    ExitCode_ConsumeEventOledHandler     = 7

} ExitCode;