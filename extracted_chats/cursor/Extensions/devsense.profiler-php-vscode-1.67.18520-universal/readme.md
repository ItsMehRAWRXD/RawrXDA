# PHP Profiler for Visual Studio Code

[![PHP Tools product page](https://img.shields.io/badge/%F0%9F%8C%8E-Home%20page-blue.svg)](https://www.devsense.com) 
[![PHP Tools on Twitter](https://badgen.net/badge/@php4vs/twitter/blue?icon=twitter)](https://twitter.com/php4vs) 

Xdebug profile files (_cachegrind format_) can be opened, viewed, and inspected. The extension also highlights **hot paths** in your code, according to the profiling results. There is more, please see the features below for the details.

---

## Overview

PHP code profiling allows you to inspect how much time and how many calls were made to every single function in the code.

[![Profiling PHP in VS Code Video](https://img.shields.io/badge/Watch%20Profiling%20on%20YouTube-red.svg?logo=youtube)](https://www.youtube.com/watch?v=VQB6pdDhGWs) 

The extension enables inspecting of Xdebug profile files which contain information about PHP code performance. It works best in combination with [PHP Tools for Visual Studio Code](https://marketplace.visualstudio.com/items?itemName=DEVSENSE.phptools-vscode). Debug, profile, and inspect the profiling results right in Visual Studio Code.

### Quickly Setup PHP for Profiling

- Ensure, you have PHP installed.
- Ensure [Xdebug](https://xdebug.org/docs/install) PHP extension is installed as well.
- Make sure PHP `zlib` extension is enabled (recommended).
- With [PHP Tools for Visual Studio Code](https://marketplace.visualstudio.com/items?itemName=DEVSENSE.phptools-vscode), use the following `launch.json` setting:
    ```json
    {
        "name": "Launch Built-in server & Profile",
        "type": "php",
        "request": "launch",
        "runtimeArgs": [
            "-S", "localhost:8888", "-t", "."
        ],
        "noDebug": true, // <-- disable debugging
        "profile": true, // <-- enable profiling
    }
    ```

## Opening Profiling Result

Open the file containing your profiling data. By default, it is in form of `cachegrind.out.%p.gz`.

**Using Command Palette**

- Bring the **Command Palette**, and run `Open Profile File (Xdebug Profiling Output)`.
- Choose the file, and confirm.
  ![open profiling file command](https://raw.githubusercontent.com/DEVSENSE/phptools-docs/master/docs/vscode/imgs/profiler-open-command.png)

**Drag & Drop "cachegrind.out.*.gz" File**

Alternatively, if your profile file name is in form of `cachegrind.out.%p.gz`, you can open the file right in the Visual Studio Code editor - using `File`/`Open File` menu.

> Be aware, large files may take some time to open.

## Inspecting Call Times

The following view lists all the functions with the number of calls (_Calls_), time spent by the function body (_Self Time_), and time including nested calls (_Time_).

![profiling call times](https://raw.githubusercontent.com/DEVSENSE/phptools-docs/master/docs/vscode/imgs/profile-calltimes.png)

## Inspecting Callers/Callees

The **Function Details** view is focused on a single function; and displays all the calling functions and all the called functions, including the time information.

![profiling callers](https://raw.githubusercontent.com/DEVSENSE/phptools-docs/master/docs/vscode/imgs/profile-details.png)

## Highlighting Hot Paths

The loaded profiling result file is analyzed, and hot paths are highlighted directly in the source code. Highlighting can be turned off in the profiling result view.

![profiling hot path highlight](https://raw.githubusercontent.com/DEVSENSE/phptools-docs/master/docs/vscode/imgs/profile-highlight.png)

## See Also

- [PHP Tools for Visual Studio](https://marketplace.visualstudio.com/items?itemName=DEVSENSE.phptools-vscode) - All-in-One PHP language integration for VS Code.
- [Profiling Documentation](https://docs.devsense.com/vscode/profiling/)
- [Documentation](https://docs.devsense.com/vscode)
- [Pricing](https://www.devsense.com/en/purchase) - Purchase PREMIUM version of PHP Tools including the Profiler.


## Notes

> The extension may send us (DEVSENSE s.r.o.) error telemetries, if enabled in VS Code (see [Telemetry settings](https://code.visualstudio.com/docs/getstarted/telemetry)). Data are anonymized, sampled, and destroyed after 30 days. This helps us to make the product stable and reliable. We do not provide the data to any 3rd party.
