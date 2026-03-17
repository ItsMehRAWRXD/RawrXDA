const fs = require('fs');
const content = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');

// Find the position of the main script block (Block 4)
const scriptMarker = '  <script>\n    // ======================================================================\n    // STATE';
const scriptPos = content.indexOf(scriptMarker);
console.log('Main script starts at byte:', scriptPos);

const htmlBeforeScript = content.substring(0, scriptPos);
const lines = htmlBeforeScript.split('\n');
console.log('HTML before script: ends at line', lines.length);

// Count div balance
const openDivs = (htmlBeforeScript.match(/<div[\s>]/gi) || []).length;
const closeDivs = (htmlBeforeScript.match(/<\/div>/gi) || []).length;
console.log('Open divs:', openDivs, 'Close divs:', closeDivs, 'Diff:', openDivs - closeDivs);

// Check for dangerous unclosed tags
const rawText = ['style', 'textarea', 'xmp', 'plaintext', 'title', 'noscript', 'noframes', 'select'];
for (const tag of rawText) {
  const opens = (htmlBeforeScript.match(new RegExp('<' + tag + '[\\s>]', 'gi')) || []).length;
  const closes = (htmlBeforeScript.match(new RegExp('</' + tag + '>', 'gi')) || []).length;
  if (opens !== closes) {
    console.log('UNCLOSED <' + tag + '>:', opens, 'opens,', closes, 'closes');
  }
}

// Check specifically for unclosed <select> which could eat the script tag
// Also check for unclosed comments
const commentOpens = (htmlBeforeScript.match(/<!--/g) || []).length;
const commentCloses = (htmlBeforeScript.match(/-->/g) || []).length;
console.log('HTML comments: opens:', commentOpens, 'closes:', commentCloses);

// Check if there's a <form> or <table> imbalance
const formOpens = (htmlBeforeScript.match(/<form[\s>]/gi) || []).length;
const formCloses = (htmlBeforeScript.match(/<\/form>/gi) || []).length;
if (formOpens !== formCloses) console.log('UNCLOSED <form>:', formOpens, 'opens,', formCloses, 'closes');
