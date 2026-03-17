import type { Disposable } from "./lifetime";
import type { QuickJSHandle } from "./types";
import type { QuickJSRuntime } from "./runtime";
import type { QuickJSContext } from "./context";
export type { PromiseExecutor } from "./types";
export declare class QuickJSDeferredPromise implements Disposable {
  owner: QuickJSRuntime;
  context: QuickJSContext;
  handle: QuickJSHandle;
  settled: Promise<void>;
  private resolveHandle;
  private rejectHandle;
  private onSettled;
  constructor(args: {
    context: QuickJSContext;
    promiseHandle: QuickJSHandle;
    resolveHandle: QuickJSHandle;
    rejectHandle: QuickJSHandle;
  });
  resolve: (value?: QuickJSHandle) => void;
  reject: (value?: QuickJSHandle) => void;
  get alive(): boolean;
  dispose: () => void;
  private disposeResolvers;
}
