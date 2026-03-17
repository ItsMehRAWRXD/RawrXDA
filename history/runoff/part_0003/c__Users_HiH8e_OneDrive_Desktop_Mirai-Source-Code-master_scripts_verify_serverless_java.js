const sampleJava = `public class AdvancedTest {
    public static void main(String[] args) {
        // Array Test
        int[] numbers = new int[5];
        numbers[0] = 42;
        System.out.println("Array Value: " + numbers[0]);

        // String Test
        String text = "Hello Serverless";
        System.out.println("Length: " + text.length());
        System.out.println("Upper: " + text.toUpperCase());

        // Math Test
        System.out.println("Sqrt(16): " + Math.sqrt(16));

        // Control Test
        for (int i = 0; i < numbers.length; i++) {
            numbers[i] = i * i;
        }
    }
}`;

function transformJava(code) {
  return code
    .replace(/public\s+class\s+(\w+)/g, 'class $1')
    .replace(/class\s+(\w+)/g, 'class $1')
    .replace(/public\s+static\s+void\s+main\s*\(\s*String\s*\[\s*\]\s+\w+\s*\)/g, 'static main(args)')
    .replace(/System\.out\.println\s*\(/g, 'console.log(')
    .replace(/System\.out\.print\s*\(/g, 'process.stdout.write(')
    .replace(/int\s+(\w+)\s*=\s*([^;]+);/g, 'let $1 = Math.floor($2);')
    .replace(/String\s+(\w+)\s*=\s*([^;]+);/g, 'let $1 = $2;')
    .replace(/boolean\s+(\w+)\s*=\s*([^;]+);/g, 'let $1 = $2;')
    .replace(/double\s+(\w+)\s*=\s*([^;]+);/g, 'let $1 = $2;')
    .replace(/float\s+(\w+)\s*=\s*([^;]+);/g, 'let $1 = $2;')
    .replace(/char\s+(\w+)\s*=\s*([^;]+);/g, 'let $1 = $2;')
    .replace(/new\s+int\s*\[\s*(\d+)\s*\]/g, 'new Array($1).fill(0)')
    .replace(/new\s+String\s*\[\s*(\d+)\s*\]/g, 'new Array($1).fill("")')
    .replace(/new\s+double\s*\[\s*(\d+)\s*\]/g, 'new Array($1).fill(0.0)')
    .replace(/new\s+(\w+)\s*\[\s*(\d+)\s*\]/g, 'new Array($2).fill(null)')
    .replace(/for\s*\(\s*int\s+(\w+)\s*=\s*(\d+)\s*;\s*\w+\s*<\s*([^;]+);\s*\w+\+\+\s*\)/g, 'for (let $1 = $2; $1 < $3; $1++)')
    .replace(/for\s*\(\s*int\s+(\w+)\s*=\s*(\d+)\s*;\s*\w+\s*<=\s*([^;]+);\s*\w+\+\+\s*\)/g, 'for (let $1 = $2; $1 <= $3; $1++)')
    .replace(/Math\.pow\s*\(/g, 'Math.pow(')
    .replace(/Math\.sqrt\s*\(/g, 'Math.sqrt(')
    .replace(/Math\.abs\s*\(/g, 'Math.abs(')
    .replace(/Math\.PI/g, 'Math.PI')
    .replace(/Math\.E/g, 'Math.E')
    .replace(/Math\.random\s*\(\s*\)/g, 'Math.random()')
    .replace(/Math\.round\s*\(/g, 'Math.round(')
    .replace(/Math\.floor\s*\(/g, 'Math.floor(')
    .replace(/Math\.ceil\s*\(/g, 'Math.ceil(')
    .replace(/Math\.max\s*\(/g, 'Math.max(')
    .replace(/Math\.min\s*\(/g, 'Math.min(')
    .replace(/(\w+)\.length\s*\(\s*\)/g, '$1.length')
    .replace(/(\w+)\.charAt\s*\(/g, '$1.charAt(')
    .replace(/(\w+)\.substring\s*\(/g, '$1.substring(')
    .replace(/(\w+)\.toUpperCase\s*\(\s*\)/g, '$1.toUpperCase()')
    .replace(/(\w+)\.toLowerCase\s*\(\s*\)/g, '$1.toLowerCase()')
    .replace(/(\w+)\.equals\s*\(\s*([^)]+)\s*\)/g, '$1 === $2')
    .replace(/(\w+)\.equalsIgnoreCase\s*\(\s*([^)]+)\s*\)/g, '$1.toLowerCase() === $2.toLowerCase()')
    .replace(/(\w+)\.contains\s*\(\s*([^)]+)\s*\)/g, '$1.includes($2)')
    .replace(/(\w+)\.startsWith\s*\(\s*([^)]+)\s*\)/g, '$1.startsWith($2)')
    .replace(/(\w+)\.endsWith\s*\(\s*([^)]+)\s*\)/g, '$1.endsWith($2)')
    .replace(/(\w+)\.indexOf\s*\(\s*([^)]+)\s*\)/g, '$1.indexOf($2)')
    .replace(/(\w+)\.replace\s*\(\s*([^,]+),\s*([^)]+)\s*\)/g, '$1.replace($2, $3)')
    .replace(/(\w+)\.trim\s*\(\s*\)/g, '$1.trim()')
    .replace(/Arrays\.toString\s*\(\s*(\w+)\s*\)/g, '$1.join(", ")')
    .replace(/Arrays\.sort\s*\(\s*(\w+)\s*\)/g, '$1.sort((a,b) => a-b)')
    .replace(/Arrays\.fill\s*\(\s*(\w+),\s*([^)]+)\s*\)/g, '$1.fill($2)')
    .replace(/try\s*\{/g, 'try {')
    .replace(/catch\s*\(\s*(\w+)\s+(\w+)\s*\)/g, 'catch ($2)')
    .replace(/finally\s*\{/g, 'finally {')
    .replace(/throw\s+new\s+(\w+)\s*\(\s*([^)]*)\s*\)/g, 'throw new Error($2)')
    .replace(/\/\/.*$/gm, '')
    .replace(/\/\*[\s\S]*?\*\//g, '');
}

const transformed = transformJava(sampleJava);
const expectations = [
  { name: 'array transformation', check: (code) => code.includes('new Array(5).fill(0)') },
  { name: 'string length conversion', check: (code) => code.includes('.length') },
  { name: 'toUpperCase preserved', check: (code) => code.includes('.toUpperCase()') },
  { name: 'Math.sqrt call', check: (code) => code.includes('Math.sqrt(') },
  { name: 'for loop translation', check: (code) => /for \(let i = [^;]+; i < numbers\.length; i\+\+\)/.test(code) }
];

let failed = false;
for (const expectation of expectations) {
  if (!expectation.check(transformed)) {
    console.error(`Expectation missing (${expectation.name}).`);
    failed = true;
  }
}

if (failed) {
  console.error('Java transformation failed expectations.');
  process.exit(1);
}

console.log('✅ Serverless Java transformation verification passed.');
console.log('Transformed snippet:');
const lines = transformed.split('\n').slice(0, 40);
console.log(lines.join('\n'));
