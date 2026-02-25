/* -------------------------------------------------------------------------- */
/*  BigDaddyG Orchestrator – Agentic Launch Panel  (self-contained)           */
/*  Usage: describe your task  →  Ctrl/Cmd + F5  (code-runner)          */
/* -------------------------------------------------------------------------- */

const BigDaddyG = (() => {
  const agents = new Map([
    ['lexer',      { name: 'Lexer',      status: 'idle', ttl: 60, exec: (t) => t.split(/\s+/)                     }],
    ['parser',     { name: 'Parser',     status: 'idle', ttl: 60, exec: (a) => ({ ast: a })                       }],
    ['semantic',   { name: 'Semantic',   status: 'idle', ttl: 60, exec: (o) => ({ ...o, types: true })           }],
    ['optimizer',  { name: 'Optimizer',  status: 'idle', ttl: 60, exec: (o) => ({ ...o, opt: true })            }],
    ['codegen',    { name: 'CodeGen',    status: 'idle', ttl: 60, exec: (o) => JSON.stringify(o, null, 2)       }],
    ['security',   { name: 'Security',   status: 'idle', ttl: 60, exec: (s) => s.replace(/password/gi, '****')  }],
    ['performance',{ name: 'Performance',status: 'idle', ttl: 60, exec: (s) => ({ len: s.length, score: 100 }) }],
    ['memory',     { name: 'Memory',     status: 'idle', ttl: 60, exec: (s) => ({ hash: s.split('').reduce((a,b)=>(a<<5)-a+b.charCodeAt(0),0) }) }],
    ['orchestrator',{name: 'Orchestrator',status:'idle',ttl:60,exec:(task)=>{
        const pipeline = ['lexer','parser','semantic','optimizer','codegen','security','performance','memory'];
        let payload = task;
        const report = [];
        for (const id of pipeline) {
            const a = agents.get(id);
            a.status = 'active';
            try { payload = a.exec(payload); report.push(`${a.name}: OK`); } catch (e) { report.push(`${a.name}: ERR ${e.message}`); }
            a.status = 'idle';
        }
        return { result: payload, report };
    }}]
  ]);

  const orchestrate = (task) => agents.get('orchestrator').exec(task);

  // Live status board (inline)
  const render = () => {
      console.clear();
      console.log('🧠 BigDaddyG – Agent Status');
      console.table([...agents.entries()].map(([k,v]) => ({ id: k, name: v.name, status: v.status, ttl: v.ttl })));
  };

  // Auto-refresh every 5 s while panel is open
  const interval = setInterval(render, 5000);
  render();

  return { orchestrate, agents, render, stop: () => clearInterval(interval) };
})();

/* -------------------------------------------------------------------------- */
/*  Example usage                                                              */
/* -------------------------------------------------------------------------- */

const task = `your task here`;  // <-- edit task or replace via snippet placeholder
const outcome = BigDaddyG.orchestrate(task);
console.log('>>> Final result:', outcome.result);
console.log('>>> Agent report:', outcome.report);

// BigDaddyG.stop(); // <-- call to halt the live status board

/* -------------------------------------------------------------------------- */
