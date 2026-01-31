type CommandHandler = (args?: any) => Promise<void> | void;

class CommandRegistry {
  private m = new Map<string, CommandHandler>();
  register(id: string, handler: CommandHandler) {
    if (this.m.has(id)) throw new Error(`Command exists: ${id}`);
    this.m.set(id, handler);
    return () => this.m.delete(id);
  }
  async execute(id: string, args?: any) {
    const fn = this.m.get(id);
    if (!fn) throw new Error(`Unknown command: ${id}`);
    return await fn(args);
  }
}
export const commands = new CommandRegistry();
