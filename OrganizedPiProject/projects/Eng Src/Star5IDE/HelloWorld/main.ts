import { commands } from './commands';

// register a hello command
commands.register('app.hello', () => alert('Hello from your mini-IDE!'));

// wire to a simple palette button (demo)
document.body.innerHTML = `
  <button id="cmd">Run app.hello</button>
  <style> body{font-family:sans-serif;padding:2rem;} button{padding:.6rem 1rem;} </style>
`;
document.getElementById('cmd')!.addEventListener('click', () => {
  commands.execute('app.hello');
});
