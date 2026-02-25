import express from "express";
import cors from "cors";
import fs from "fs";
import path from "path";
import { exec } from "child_process";

const app = express();
app.use(cors());
app.use(express.json({limit:"200mb"}));

app.post("/fs/read",(req,res)=>{
    const p = req.body.path;
    const items = fs.readdirSync(p,{withFileTypes:true}).map(d=>({
        name:d.name,
        path:path.join(p,d.name),
        isDir:d.isDirectory()
    }));
    res.json(items);
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

