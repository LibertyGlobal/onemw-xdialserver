/*

to run this on the box, eg:

CPE_HOST=127.0.0.1 NODE_PATH=/usr/lib/node_modules node /media/mass_storage/xdial-app-test-server.js

*/

const ws = require("nodejs-websocket");
const { resolve } = require("path");
const process = require("process")

process.on('SIGINT', function () {
    console.info("SIGINT handler!");
    if (connection) {
        console.info('unregistering ...');
        for (let event of ["onApplicationLaunchRequest", "onApplicationHideRequest", "onApplicationResumeRequest", "onApplicationStopRequest", "onApplicationStateRequest"]) {
            connection.send(`{"jsonrpc": "2.0", "id": 0, "method": "org.rdk.Xcast.1.unregister", "params": {"event":"${event}","id":"bumshakalaka"}}`, () => console.info(`unregistered from ${event}`));
        }
    }
    process.exit(0);
});


const IP = process.env.CPE_HOST
if (!IP) {
    console.error("provide some IP via CPE_HOST");
    process.exit(1);
}

// application name -> Application
const APPPLICATIONS = {};

const STATES = { "running": "running", "stopped": "stopped", "hidden": "hidden" }
const ACTIVITIES = { "starting":"starting", "hiding": "hiding", "stopping":"stopping"}

class Application {
    constructor(app, id) {
        this.state = STATES.stopped;
        this.app = app;
        this.id = id;
        this.reqid = 0;
    }

    async performAction(promise) {
        if (this.currentAction) {
            this.currentAction = this.currentAction.then(promise);
        } else {
            this.currentAction = promise;
        }
    }

    onApplicationLaunchRequest(params) {
        // console.info(`> ${this.app}: onApplicationLaunchRequest`);
        const self = this; console.info(`LAUNCH, SELF: ${JSON.stringify(self)}`);
        if (this.state != STATES.stopped) {
            console.info(`trying to launch while state is ${this.state}; return ...`)
            return;
        }

        this.performAction(new Promise((resolve, reject) => {
            if (self.activity) {
                console.info(`wanted to launch, but already ${self.activity}`);
                resolve();
            }
            self.activity = ACTIVITIES.starting;
            setTimeout(() => {
                if (self.activity == ACTIVITIES.starting) {
                    const success = true;
                    console.info(`onApplicationLaunchRequest success: ${success}`);
                    self.state = STATES.running;
                    self.sendStateUpdate();
                    self.activity = null;
                }
                resolve();
            }, 5000);
        }));
    }

    onApplicationHideRequest(params) {
        // console.info(`> ${this.app}: onApplicationHideRequest`);
        const self = this; console.info(`HIDE, SELF: ${JSON.stringify(self)}`);
        if (this.state != STATES.running) {
            console.info(`trying to hide while state is ${this.state}; return ...`)
            return;
        }

        this.performAction(new Promise((resolve, reject) => {
            if (self.activity) {
                console.info(`wanted to hide, but already ${self.activity}`);
                resolve();
            }
            self.activity = ACTIVITIES.hiding;
            self.statetransition = true;
            setTimeout(() => {
                if (self.activity == ACTIVITIES.hiding) {
                    const success = true;
                    console.info(`onApplicationHideRequest success: ${success}`);
                    self.state = STATES.hidden;
                    self.sendStateUpdate();
                    self.statetransition = false;
                    self.activity = null;
                }
                
                resolve();
            }, 2000);
        }));
    }
    onApplicationResumeRequest(params) {
        console.info(`> ${this.app}: onApplicationResumeRequest`);
    }
    onApplicationStopRequest(params) {
        // console.info(`> ${this.app}: onApplicationStopRequest`);
        const self = this; console.info(`STOP, SELF: ${JSON.stringify(self)}`);
        if (this.state == STATES.stopped) {
            console.info(`trying to stop while state is ${this.state}; return ...`)
            return;
        }
        this.performAction(new Promise((resolve, reject) => {
            self.activity = STATES.stopping;
            setTimeout(() => {
                const success = true;
                console.info(`onApplicationStopRequest success: ${success}`);
                self.state = STATES.stopped;
                self.sendStateUpdate();
                self.activity = null;
                resolve();
            }, 5000);
        }));
    }
    onApplicationStateRequest(params) {
        // console.info(`> ${this.app}: onApplicationStateRequest`);
        setTimeout(() => {
            this.sendStateUpdate();
        }, 250);
    }

    sendStateUpdate() {
        const response = { "jsonrpc": "2.0", "id": ++this.reqid, "method": "org.rdk.Xcast.1.onApplicationStateChanged", "params": { "applicationName": this.app, "state": STATES[this.state], "applicationId": this.id } };
        connection.sendText(JSON.stringify(response));
    }
}

/* options is an object that will be passed to net.connect() (or tls.connect() if the protocol is "wss:").
The properties "host" and "port" will be read from the URL. The optional property extraHeaders will be used
to add more headers to the HTTP handshake request. If present, it must be an object, like {'X-My-Header': 'value'}.
The optional property protocols will be used in the handshake (as "Sec-WebSocket-Protocol" header) to allow the
server to choose one of those values. If present, it must be an array of strings.*/
const URL = `ws://${IP}:9998/Service/org.rdk.Xcast`;
console.info(`about to connect to ${URL}`)
let connection = ws.connect(URL, { "protocols": ["jsonrpc"] }, () => {
    console.info("Connected!");
    for (let event of ["onApplicationLaunchRequest", "onApplicationHideRequest", "onApplicationResumeRequest", "onApplicationStopRequest", "onApplicationStateRequest"]) {
        connection.send(`{"jsonrpc": "2.0", "id": 0, "method": "org.rdk.Xcast.1.register", "params": {"event":"${event}","id":"bumshakalaka"}}`, () => console.info(`registered for ${event}`));
    }
    connection.on("text", (str) => {
        console.info(`recv: ${str}`);
        // {"jsonrpc":"2.0","method":"bumshakalaka.onApplicationStateRequest","params":{"applicationName":"YouTube","applicationId":""}}
        try {
            const request = JSON.parse(str);
            if (request.method && request.method.indexOf("bumshakalaka.") == 0) {
                const method = request.method.substr("bumshakalaka.".length);
                const applicationId = request.params.applicationId;
                const appName = request.params.applicationName;
                
                // we disregard app id, since it is sometimes "" (on launch request, on onApplicationStateRequest), sometimes "0" (on stop/hide)
                // is it actually correct?
                const appWithId = appName; // `${appName}/${applicationId || "DEFAULT"}`;

                // console.info(`request: ${method} for ${appWithId}`);
                if (!APPPLICATIONS[appWithId]) {
                    APPPLICATIONS[appWithId] = new Application(appName, applicationId);
                }
                APPPLICATIONS[appWithId][method](request.params);
            }
        } catch (e) {
            console.error(`error handling request: ${e}`);
        }
    });
});