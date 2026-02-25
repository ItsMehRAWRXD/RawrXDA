import express from "express";
import cors from "cors";
import fs from "fs";
import path from "path";
import { exec } from "child_process";
import { fileURLToPath } from "url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const app = express();
app.use(cors());
app.use(express.json({limit:"200mb"}));
app.use(express.static(path.resolve(__dirname, "../ui")));

app.post("/fs/read",(req,res)=>{
    const p = req.body.path;
    const items = fs.readdirSync(p,{withFileTypes:true}).map(d=>({
        name:d.name,
        path:path.join(p,d.name),
        isDir:d.isDirectory()
    }));
    res.json(items);
});

app.post("/fs/file/read",(req,res)=>{
    const p = req.body.path;
    try {
        const content = fs.readFileSync(p, "utf8");
        res.json({ path:p, content });
    } catch (error) {
        res.status(500).json({ error:error.message });
    }
});

app.post("/fs/file/write",(req,res)=>{
    const { path: target, content } = req.body;
    try {
        fs.writeFileSync(target, content, "utf8");
        res.json({ ok:true });
    } catch (error) {
        res.status(500).json({ error:error.message });
    }
});

app.post("/exec",(req,res)=>{
    exec(req.body.cmd,(err,out)=>res.send(out||err));
});

app.post("/v1/chat/completions",(req,res)=>{
    const { messages } = req.body;
    const text = messages[messages.length-1].content;
    res.json({
        choices:[{
            message:{ role:"assistant", content:`Processed: ${text}` }
        }]
    });
});

app.listen(11442,()=>console.log("Backend 11442"));

