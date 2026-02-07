import * as readline from 'readline';
import { setProvider, setApiKey, getProviders, createProvider } from './ai/provider';

const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout,
  prompt: '> '
});

function handleCommand(input: string): void {
  const parts = input.trim().split(' ');
  const cmd = parts[0];

  switch (cmd) {
    case '/key':
      if (parts.length < 3) {
        console.log('Usage: /key <provider> <api-key>');
        return;
      }
      setApiKey(parts[1], parts.slice(2).join(' '));
      console.log(`API key set for ${parts[1]}`);
      break;

    case '/switch':
      if (parts.length < 2) {
        console.log('Usage: /switch <provider>');
        return;
      }
      setProvider(parts[1]);
      console.log(`Switched to ${parts[1]} provider`);
      break;

    case '/complete':
      if (parts.length < 2) {
        console.log('Usage: /complete "<prompt>"');
        return;
      }
      const prompt = parts.slice(1).join(' ').replace(/^"|"$/g, '');
      complete(prompt);
      break;

    case '/providers':
      console.log('Available providers:', getProviders().join(', '));
      break;

    default:
      console.log('Unknown command. Available: /key, /switch, /complete, /providers');
  }
}

async function complete(prompt: string): Promise<void> {
  try {
    const provider = createProvider();
    const response = await provider.respond({
      messages: [{ role: 'user', content: prompt }]
    });
    console.log(response);
  } catch (error) {
    console.error('Error:', error instanceof Error ? error.message : error);
  }
}

console.log('AI Editor CLI - Type commands or /help');
rl.prompt();

rl.on('line', (input) => {
  if (input.trim() === '/exit') {
    rl.close();
    return;
  }
  handleCommand(input);
  rl.prompt();
});

rl.on('close', () => {
  console.log('Goodbye!');
  process.exit(0);
});