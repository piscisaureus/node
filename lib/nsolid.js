(function(f){if(typeof exports==="object"&&typeof module!=="undefined"){module.exports=f()}else if(typeof define==="function"&&define.amd){define([],f)}else{var g;if(typeof window!=="undefined"){g=window}else if(typeof global!=="undefined"){g=global}else if(typeof self!=="undefined"){g=self}else{g=this}g.nsolid = f()}})(function(){var define,module,exports;return (function e(t,n,r){function s(o,u){if(!n[o]){if(!t[o]){var a=typeof require=="function"&&require;if(!u&&a)return a(o,!0);if(i)return i(o,!0);var f=new Error("Cannot find module '"+o+"'");throw f.code="MODULE_NOT_FOUND",f}var l=n[o]={exports:{}};t[o][0].call(l.exports,function(e){var n=t[o][1][e];return s(n?n:e)},l,l.exports,e,t,n,r)}return n[o].exports}var i=typeof require=="function"&&require;for(var o=0;o<r.length;o++)s(r[o]);return s})({1:[function(require,module,exports){
"use strict"

const activeHandles = require("active-handles/core")
const activeRequests = require("active-requests/core")
const binding = global.process._linkedBinding('nsolid_agent');
//var activeStreams = require("active-streams/core")

exports.init = init

function init() {
  binding.registerJSCommand("/async_activity", getAsyncActivity, true)
}

function getAsyncActivity(req) {
  req.return(new AsyncWork());
}

function AsyncWork() {
  this.handles = activeHandles()
  this.requests = activeRequests()
  //this.streams = activeStreams()
}

},{"active-handles/core":10,"active-requests/core":11}],2:[function(require,module,exports){
"use strict";
exports.async_activity = require("./async_activity")
exports.info = require("./info")
exports.ping = require("./ping")
exports.profile = require("./profile")
exports.stats = require("./stats")

},{"./async_activity":1,"./info":3,"./ping":4,"./profile":5,"./stats":6}],3:[function(require,module,exports){
"use strict"

const process = global.process;
const binding = process._linkedBinding('nsolid_agent');
const pid = process.pid;
const NODE_ENV = process.env.NODE_ENV;
const execPath = process.execPath;
function getMain() {
  if (process.argv.length < 2) return '';
  try {
    return process.argv[1] ? require('path').resolve(process.argv[1]) : '';
  }
  // ENOENT on CWD
  catch (e) {
    return '';
  }
}
const main = getMain(); 

exports.init = init

// static
function init(eventBus) {
  const info = {
    id: eventBus.id,
    app: eventBus.appName,
    pid: pid,
    nodeEnv: NODE_ENV,
    execPath: execPath,
    main: main
  }
  binding.registerStaticCommand("/info", info);
}

},{"path":undefined}],4:[function(require,module,exports){
"use strict"

const binding = global.process._linkedBinding('nsolid_agent');
exports.init = function () {
  binding.registerStaticCommand("/ping", "PONG");
}

},{}],5:[function(require,module,exports){
"use strict"

exports.init = init

/*
  Installs the `profile_start` `profile_stop` handlers 
*/

const profiler = require("profiler")
const process = global.process;
const binding = process._linkedBinding('nsolid_agent');
const setTimeout = global.setTimeout;
const clearTimeout = global.clearTimeout;
const Error = global.Error;

var debug = require("util").debuglog("nsolid")

function init() {
  binding.registerJSCommand("/profile_start", start, true)
  binding.registerJSCommand("/profile_stop", stop, true)
}

// TODO determine correct default value
const PROFILER_TIMEOUT = 70000
var timeout;
const startProfiling = profiler.startProfiling.bind(profiler);
const stopProfiling = profiler.stopProfiling.bind(profiler);
function start(req) {
  if (timeout) {
    var err = {
      message: "Profile already in progress"
    };
    req.throw(err);
    return
  }

  debug("starting cpu profile collection")
  startProfiling()

  // set time limit
  timeout = setTimeout(function () {
    if (timeout) {
      debug("collection timed out, aborting profiling")
      stopProfiling()
      timeout = null
    }
  }, PROFILER_TIMEOUT)
  timeout.unref()

  req.return({
    started: Date.now(),
    collecting: true
  })
}

function stop(req) {
  if (!timeout) {
    var err = new Error("Not currently collecting")
    err.status = 428
    req.throw(err);
    return;
  }

  clearTimeout(timeout)
  timeout = null
  debug("collecting profile data for reply")
  const profile = stopProfiling()

  req.return(profile)
}

},{"profiler":undefined,"util":undefined}],6:[function(require,module,exports){
"use strict"

exports.init = init

const process = global.process;
const call = Function.call.bind(Function.call); 
const bind = Function.call.bind(Function.bind); 
const reduce = bind(call.bind(null, Array.prototype.reduce), null);
const forEach = bind(call.bind(null, Array.prototype.forEach), null);
const keys = bind(Object.keys, Object)

const os = require("os")
const cpus = bind(os.cpus, os);
const pidusage = require("pidusage").stat
const binding = process._linkedBinding('nsolid_agent');
const memoryUsage = bind(process.memoryUsage, process);
const _getActiveRequests = bind(process._getActiveRequests, process);
const _getActiveHandles = bind(process._getActiveHandles, process);
const env = process.env;
const loadavg = call.bind(null, os.loadavg, os);
const freemem = call.bind(null, os.freemem, os);
const uptime = call.bind(null, os.uptime, os);
const processUptime = call.bind(null, process.uptime, process);

const nsolid_versions = require("nsolid_versions");
const hostname = bind(os.hostname, os);
const totalmem = bind(os.totalmem, os);
const arch = process.arch;
const platform = process.platform;
const startupTimes = bind(process.startupTimes, process);

/*
  Installs the "info", "versions", "system_stats", "process_stats", "startup_times" handlers.
*/

function init() {
  binding.registerStaticCommand("/system_info", staticInfo());
  binding.registerStaticCommand("/versions", versions());
  binding.registerJSCommand("/system_stats", systemStats, true);
  binding.registerJSCommand("/process_stats", processStats);
  binding.registerJSCommand("/startup_times", function (req) {
    req.return(startupTimes());
  }, true)
}

// static
function versions() {
  const static_versions = {}
  forEach(keys(process.versions), function (dep) {
    static_versions[dep] = process.versions[dep]
  })
  static_versions.nsolid_lib = nsolid_versions; 
  return static_versions
}

// static
function staticInfo() {
  const cpuinfo = cpus()
  const info = {
    arch: arch,
    platform: platform,
    hostname: hostname(),
    totalmem: totalmem()
  }

  if (cpuinfo) {
    info.cpu_cores = cpuinfo.length
    info.cpu_model = cpuinfo[0].model
  }

  return info
}

function sumSpeed(prev, curr) {
  return prev + curr.speed
}

function systemStats(req) {
  const load = loadavg()
  const cpuinfo = cpus()
  const stats = {
    freemem: freemem(),
    uptime: uptime(),
    load_1m: load[0],
    load_5m: load[1],
    load_15m: load[2]
  }
  // in here vs staticInfo because of dynamic cpu frequency scaling
  if (cpuinfo) {
    stats.cpu_speed = reduce(cpuinfo, sumSpeed, 0) / cpuinfo.length
  }

  req.return(stats);
}

function processStats(req) {
  pidusage(process.pid, function(err, usage) {
    if (err) {
      req.throw({
        message: err.message
      });
    }
    var stats;
    try {
      const memInfo = memoryUsage()

      stats = {
        uptime: processUptime(),
        rss: memInfo.rss,
        heapTotal: memInfo.heapTotal,
        heapUsed: memInfo.heapUsed,
        active_requests: _getActiveRequests().length,
        active_handles: _getActiveHandles().length,
        user: env.USER,
        title: process.title,
        cpu: usage.cpu
      }
    }
    catch (ex) {
      req.throw({
        message: ex.message
      });
      return;
    }
    req.return(stats);
  });

}

},{"nsolid_versions":undefined,"os":undefined,"pidusage":12}],7:[function(require,module,exports){
"use strict";

const encodeURIComponent = global.encodeURIComponent;
const stringify = global.JSON.stringify.bind(global.JSON);
const pid = global.String(global.process.pid);
const hostname = require('os').hostname();

function encode(str) {
    return encodeURIComponent(str).replace(/[%]/g, '%%');
}

exports.getOptions = function (name, id) {
  return {
    etcdPayloadTemplate: 'value=' +
        encode('{' +
          '"pid":' + pid + ',' +
          '"hostname":' + stringify(hostname) + ',' +
          '"app":' + stringify(name) +',' +
          '"address":"') + '%s' + encode(':') + '%i' +
                      encode('",' +
          '"id":' + stringify(id) +
        '}') +
        '&ttl=%i',
    etcdPathname: "/v2/keys/nsolid/"+encodeURIComponent(name)+"/"+encodeURIComponent(id)
  };
}


},{"os":undefined}],8:[function(require,module,exports){
"use strict";
module.exports = id

const crypto = require("crypto")
const createHash = crypto.createHash.bind(crypto);
const randomBytes = crypto.randomBytes.bind(crypto);
const os = require("os")
const hostname = os.hostname.bind(os);
const String = global.String
const pid = String(global.process.pid)
const now = global.Date.now.bind(global.Date);

function id() {
  const sha = createHash("sha1");
  sha.update(randomBytes(20));
  sha.update(hostname());
  sha.update(String(now()));
  sha.update(String(pid));
  const digest = sha.digest("hex");
  return digest;
}

},{"crypto":undefined,"os":undefined}],9:[function(require,module,exports){
"use strict";
const isIPv4 = require("net").isIPv4;
const isIPv6 = require("net").isIPv6;
const parseFloat = global.parseFloat;
//const String = global.String;
//const Number = global.Number;
exports.listen_options = listen_options;

const ipv6_group = '(?:\\[([^\\]]*)\\])';
const ipv4_group = '(\\d{1,3}(?:\\.\\d{1,3}){3})';
const port_group = '(\\d{1,5}|\\?)';
const pattern = new RegExp(
  '^(?:' +
  '(?:(?:' + ipv6_group + '|' + ipv4_group + ')(?:[:]?' + port_group + ')?)|' +
  '[:]?' + port_group +
  ')$'
)
const exec_pattern = pattern.exec.bind(pattern);
function listen_options(socket) {
  if (typeof socket !== 'string' || socket === '') {
    return null;
  }
  // port
  // ip:port
  // ip = [ipv6] | ipv4
  // ip:?
  // ip:
  const listenOptions = {
    host: '0.0.0.0',
    port: 0
  }
  const matches = exec_pattern(socket);
  // not an ip, hostname?
  if (matches === null) {
    var index = socket.lastIndexOf(':');
    if (index === -1) {
      listenOptions.host = socket;
    }
    else {
      listenOptions.host = socket.slice(0, index);
      listenOptions.port = socket.slice(index + 1);
      if (listenOptions.port === '') {
        //console.error('malformed address: %j, dropping', socket);
        return null;
      }
    }
  }
  else {
    var ip;
    // ipv6
    if (matches[1]) {
      ip = matches[1];
      if (!isIPv6(ip)) {
        //console.error('malformed IPv6 address: %j, dropping', socket);
        return null;
      }
      listenOptions.host = ip;
    }
    // ipv4
    else if (matches[2]) {
      ip = matches[2];
      if (!isIPv4(ip)) {
        //console.error('malformed IPv4 address: %j, dropping', socket);
        return null;
      }
      listenOptions.host = ip;
    }
    // port attached to ip
    if (matches[3]) {
      listenOptions.port = matches[3]; 
    }
    // only port
    else if (matches[4]) {
      listenOptions.port = matches[4];
    }
  }
  if (listenOptions.port === '?') {
    listenOptions.port = 0;
  }
  else {
    listenOptions.port = parseFloat(listenOptions.port, 10);
  }
  return listenOptions;
}

},{"net":undefined}],10:[function(require,module,exports){
'use strict';

var getFunctionLocation = require('_function_origin')
  , format              = require('util').format

var handleType = {
    unknown             : { flag : 0x0  , name : 'unknown' }
  , timeout             : { flag : 0x1  , name : 'setTimeout' }
  , interval            : { flag : 0x2  , name : 'setInterval' }
  , tcpServerConnection : { flag : 0x4  , name : 'TCP server connection' }
  , tcpSocketConnection : { flag : 0x8  , name : 'TCP socket connection' }
  , tcpClientRequest    : { flag : 0x10 , name : 'TCP client request' }
  , childProcessSpawn   : { flag : 0x20 , name : 'spawned child process' }
  , pipeSocket          : { flag : 0x40 , name : 'pipe socket' }
  , pipe                : { flag : 0x80 , name : 'IPC pipe' }
  , fsWatcher           : { flag : 0x100, name : 'file watcher' }
}

function getPrototypeName(obj) {
  if (typeof obj !== 'object' || obj === null) return '!not-an-object!';
  return Object.getPrototypeOf(obj).constructor.name;
}

function isPrototype(obj, s) {
  if (typeof obj !== 'object' || obj === null) return false;
  return getPrototypeName(obj) === s;
}

function getSocketHandleDetails(handle) {
  var peername      = handle._getpeername()
    , sockname      = handle._getsockname()

  return {
      fd              : handle._handle.fd
    , transport       : getPrototypeName(handle._handle)
    , bytesDispatched : handle._bytesDispatched
    , connections     : handle._connections
    , hadError        : handle._hadError
    , pendingData     : handle._pendingdata
    , host            : handle._host
    , peerAddress     : peername.address
    , peerFamily      : peername.family
    , peerPort        : peername.port
    , sockAddress     : sockname.address
    , sockFamily      : sockname.family
    , sockPort        : sockname.port
    , ondata          : locationStringOfFn(handle._events.data, [ '_http_client.js', 'child_process.js', '_stream_readable.js' ])
    , onclose         : locationStringOfFn(handle._events.close, [ '_http_client.js', '_http_agent.js', 'child_process.js', 'events.js' ])
    , onend           : locationStringOfFn(handle._events.end, [ '_http_client.js', 'child_process.js', 'events.js' ])
    , onfinish        : locationStringOfFn(handle._events.finish, [ 'net.js', 'events.js' ])
  }
}

function locationString(loc) {
  return loc
    ? format('%s:%d:%d', loc.file, loc.line, loc.column)
    : 'Unknown location';
}

function functionNameInfo(fn, location) {
  var anonymous, name;
  if (!fn.name || !fn.name.length) {
    name = location.inferredName && location.inferredName.length
        ? location.inferredName
        : '__unknown_function_name__';

    anonymous = true;
  } else {
    name = fn.name;
    anonymous = false;
  }

  return { name: name, anonymous: anonymous };
}

function locationStringOfFn(fn, ignoreFile) {
  var currentFn, i, len;
  // in case registered events, it may be one or a list
  if (Array.isArray(fn)) {
    for (i = 0, len = fn.length; i < len; i++) {
      currentFn = locationStringOfFn(fn[i], ignoreFile);
      if (currentFn) return currentFn;
    }
    return;
  }

  if (typeof fn !== 'function') return;
  var loc = getFunctionLocation(fn, null, true);

  // ignore either one file or multiples
  if (typeof ignoreFile === 'string' && loc.file === ignoreFile) return ;
  if (Array.isArray(ignoreFile)) {
    for (i = 0, len = ignoreFile.length; i < len; i++) {
      if (loc.file === ignoreFile[i]) return;
    }
  }

  var nameInfo = functionNameInfo(fn, loc);
  return nameInfo.name + ': ' + locationString(loc);
}

function notCore(fn) {
  if (typeof fn !== 'function') return;

  var file = getFunctionLocation(fn).file;
  if ((/^(internal\/)?[^/\\]+\.js$/).test(file)) return;
  return fn;
}

function firstNonCoreFunction(obj, names) {
  var fn;
  // all functions inside core are missing any path indicators like / or \ (windows)
  for (var i = 0, len = names.length; i < len; i++) {
    fn = obj[names[i]];

    // in case registered events, it may be one or a list
    if (Array.isArray(fn)) {
      for (var j = 0; j < fn.length; j++) {
        if (notCore(fn[j])) return fn[j];
      }
    } else if (notCore(fn)) {
      return fn;
    }
  }
}


function HandleResolver(opts) {
  if (!(this instanceof HandleResolver)) return new HandleResolver(opts);

  this._opts = opts;
  this._resolvedHandles = [];
  this._fileDescriptors = {};
}

var proto = HandleResolver.prototype;

proto._functionInfo = function _functionInfo(fn, handle, locationOnly) {
  if (typeof fn !== 'function') return {};

  var location = getFunctionLocation(fn)
  // v8 zero bases lines
  if (location) location.line++;

  var nameInfo = functionNameInfo(fn, location);

  // handle anonymous functions and try to figure out a meaningful function name
  var ret = {
      fn          : fn
    , name        : nameInfo.name
    , location    : location
    , anonymous   : nameInfo.anonymous
  }

  // allow local overrides, otherwise add source
  // if any feature specified in opts needs it
  if (!locationOnly && (this._opts.highlight || this._opts.source)) ret.source = fn.toString();
  if (!locationOnly && this._opts.attachHandle) ret.handle = handle;

  return ret;
}

proto._addFunctionInfo = function _addFunctionInfo(fn, handle, type) {
  var info = this._functionInfo(fn, handle);
  info.type = type || handleType.unknown;
  info.details = {};
  this._resolvedHandles.push(info);
  return info;
}

proto._resolveTimerHandles = function _resolveTimerHandles(handle) {
  var addedInfo, type, fn, visited = {};
  // timer handles created via setTimeout or setInterval
  for (var next = handle._idleNext; !!next && !visited[next]; next = next._idleNext) {
    visited[next] = true;
    var repeatIsFn = typeof next._repeat === 'function';
    var hasWrappedCallback = typeof next._wrappedCallback === 'function';

    if (!repeatIsFn && !hasWrappedCallback && !next.hasOwnProperty('_onTimeout')) continue;

    // starting with io.js 1.6.2 when using setInterval the timer handle's
    // _repeat property references the wrapped function so we prefer that
    fn = repeatIsFn
        ? next._repeat
        : hasWrappedCallback ? next._wrappedCallback : next._onTimeout;


    type = hasWrappedCallback || repeatIsFn
        ? handleType.interval
        : handleType.timeout;

    addedInfo = this._addFunctionInfo(fn, handle, type);
    addedInfo.details = { msecs: next._idleTimeout }
  }
  return !!addedInfo;
}

proto._resolveTcpServerHandle = function _resolveTcpServerHandle(handle) {
  if (!isPrototype(handle, 'Server') || !handle._handle) return false;
  var fd            = handle._handle.fd
    , type          = handleType.tcpServerConnection
    , connectionKey = handle._connectionKey
    , fn            = handle._events.listening
                   || handle._events.request
                   || handle._events.connection
                   || handle._handle.onconnection


  if (typeof fn !== 'function') return false; // not sure what this is

  var addedInfo = this._addFunctionInfo(fn, handle, type);
  addedInfo.details = {
      fd            : fd
    , transport     : getPrototypeName(handle._handle)
    , connectionKey : connectionKey
    , connections   : handle._connections
  }
  return true;
}

proto._resolveTcpSocketClientRequestHandle = function _resolveTcpSocketClientRequestHandle(handle) {
  if ( !isPrototype(handle, 'Socket')
    || !handle._handle
    || !isPrototype(handle._httpMessage, 'ClientRequest')) return false;

  var type = handleType.tcpClientRequest
    , req = handle._httpMessage
    , fn = req._events.response

  if (typeof fn !== 'function') return false;

  var addedInfo = this._addFunctionInfo(fn, handle, type);
  var details = getSocketHandleDetails(handle);
  // The below are details concerning the **outgoing** request
  // more details are available, i.e. req._header (entire header string)
  // let's see what people need at first and/or introduce opts.verbose feature
  details.reqHasBody         = req._hasBody;
  details.reqHeadersSent     = req.headersSent;
  details.reqProtocol        = req.agent && req.agent.protocol;
  details.reqMethod          = req.method;
  details.reqPath            = req.path;
  details.reqShouldKeepAlive = req.shouldKeepAlive;
  // finished becomes true when `outgoingMessage.end()` was called
  details.reqFinished        = req.finished;

  addedInfo.details = details;
  return true;
}

proto._resolveTcpSocketHandle = function _resolveTcpSocketHandle(handle) {
  if (!isPrototype(handle, 'Socket') || !handle._handle) return false;
  var type  = handleType.tcpSocketConnection
    , fn    = handle._events.connection
    , isTCP = isPrototype(handle._handle, 'TCP')

  // fn is not always given, i.e. in underlying TCP sockets
  if (!fn && !isTCP) return;

  var addedInfo = this._addFunctionInfo(fn, handle, type);
  addedInfo.details = getSocketHandleDetails(handle);
  return true;
}

proto._resolveChildProcessHandle = function _resolveChildProcessHandle(handle) {
  if (!isPrototype(handle, 'ChildProcess') || !handle._handle) return false;
  var type = handleType.childProcessSpawn

  var cmd        = handle.spawnargs.join(' ')
    , stdin      = handle.stdin
    , stdout     = handle.stdout
    , stderr     = handle.stderr
    , events     = handle._events
    , channel_fd = handle._channel && handle._channel.fd
    , stdin_fd   = stdin  && stdin._handle  && stdin._handle.fd
    , stdout_fd  = stdout && stdout._handle && stdout._handle.fd
    , stderr_fd  = stderr && stderr._handle && stderr._handle.fd

  // First choice for fn is the callback given by user via exec
  // this will not be present if the process was spawned directly instead
  // XXX:  The _callback property was added to nsolid only for now
  var fn = handle._callback;

  // If no _callback is found, try to find a registered event.
  // The user could register multiple functions with a child_process,
  // 'error', 'exit', 'close', 'disconnect', 'message'.
  // Providing detailed fn info for one should be sufficient in order to find origin of the handle
  if (!fn) fn = firstNonCoreFunction(events, [ 'close', 'exit', 'disconnect', 'message', 'error' ]);

  var addedInfo = this._addFunctionInfo(fn, handle, type);

  var details = {
      command        : cmd
    , pid            : handle.pid
    , connected      : handle.connected
    , closesGot      : handle._closesGot
    , closesNeeded   : handle._closesNeeded
    , exitCode       : handle.exitCode

    // include location string for all possible event handlers
    , onclose        : locationStringOfFn(events.close, 'child_process.js')
    , onexit         : locationStringOfFn(events.exit)
    , ondisconnect   : locationStringOfFn(events.disconnect)
    , onmessage      : locationStringOfFn(events.message)
    , onerror        : locationStringOfFn(events.error, 'child_process.js')

    // providing info about the pipes should help as well to track down
    // the origin of a child process handle
    // more details about these functions will be resolved when each socket
    // handle is processed
    , stdin_fd       : stdin_fd
    , stdout_fd      : stdout_fd
    , stderr_fd      : stderr_fd
    // fork had IPC _channel
    , channel_fd     : channel_fd
  };

  // in some cases like `fork` the pipes are not present
  if (stdin && stdin._eventsCount) {
    details.stdin_ondata   = locationStringOfFn(stdin._events.data, 'child_process.js')
    details.stdin_onclose  = locationStringOfFn(stdin._events.close, 'child_process.js')
    details.stdin_onfinish = locationStringOfFn(stdin._events.finish, 'net.js')
  }
  if (stdout && stdout._eventsCount) {
    details.stdout_ondata  = locationStringOfFn(stdout._events.data, 'child_process.js')
    details.stdout_onend   = locationStringOfFn(stdout._events.end, 'events.js')
    details.stdout_onclose = locationStringOfFn(stdout._events.close, 'child_process.js')
  }
  if (stderr && stderr._eventsCount) {
    details.stderr_ondata  = locationStringOfFn(stderr._events.data, 'child_process.js')
    details.stderr_onend   = locationStringOfFn(stderr._events.end, 'events.js')
    details.stderr_onclose = locationStringOfFn(stderr._events.close, 'child_process.js')
  }

  addedInfo.details = details;

  // store map of fds to stdin/stdout/stderr to use for socket pipe resolution
  if (stdin_fd) this._fileDescriptors[stdin_fd] = 'stdin';
  if (stdout_fd) this._fileDescriptors[stdout_fd] = 'stdout';
  if (stderr_fd) this._fileDescriptors[stderr_fd] = 'stderr';
  if (channel_fd) this._fileDescriptors[channel_fd] = 'channel';
  return true;
}

proto._resolvePipeSocketHandle = function _resolvePipeSocketHandle(handle) {
  if ( !isPrototype(handle, 'Socket')
    || !handle._handle
    || !isPrototype(handle._handle, 'Pipe')) return false;

  var type   = handleType.pipeSocket
    , pipe   = handle._handle
    , events = handle._events
    , fn     = firstNonCoreFunction(events, [ 'close', 'data', 'end', 'finish', 'error' ]);

  var addedInfo = this._addFunctionInfo(fn, handle, type);
  var details = getSocketHandleDetails(handle);
  details.reading = pipe.reading;
  if (details.fd) details.name = this._fileDescriptors[details.fd];
  addedInfo.details = details;
  return true;
}

proto._resolvePipeHandle = function _resolvePipeHandle(handle) {
  if (!isPrototype(handle, 'Pipe')) return false;
  var addedInfo = this._addFunctionInfo(null, handle, handleType.pipe);
  addedInfo.details = {
      fd             : handle.fd
    , writeQueueSize : handle.writeQueueSize
    , buffering      : handle.buffering
    , name           : this._fileDescriptors[handle.fd]
  }
  return true;
}

proto._resolveFSWatcherHandle = function _resolveFSWatcherHandle(handle) {
  if (!isPrototype(handle, 'FSWatcher')) return false;

  var type   = handleType.fsWatcher
    , events = handle._events

  var fn = handle._eventsCount
      ? firstNonCoreFunction(events, [ 'change', 'close', 'error' ])
      : null;

  var addedInfo = this._addFunctionInfo(fn, handle, type);

  if (handle._eventsCount) {
      addedInfo.details = {
        onchange  : locationStringOfFn(events.change, 'fs.js')
      , onclose : locationStringOfFn(events.close, 'fs.js')
      , onerror : locationStringOfFn(events.error, 'fs.js')
    }
  }
  return true
}

proto._debugUnknownHandle = function _debugUnknownHandle(handle) {
  if (!process.env.ACTIVE_HANDLES_DEBUG) return;
  function inspect(obj, depth) {
    console.error(require('util').inspect(obj, false, depth || 10, true));
  }
  var unknown = {};
  unknown[getPrototypeName(handle)] = handle;
  inspect(unknown);
}

proto._addHandle = function _addHandle(handle) {
  // try each resolver until one succeeds to resolve the handle
  // and adds the info to _resolvedHandles
  return this._resolveTimerHandles(handle)
      || this._resolveTcpServerHandle(handle)
      || this._resolveTcpSocketClientRequestHandle(handle)
      || this._resolveTcpSocketHandle(handle)
      || this._resolveChildProcessHandle(handle)
      || this._resolvePipeSocketHandle(handle)
      || this._resolvePipeHandle(handle)
      || this._resolveFSWatcherHandle(handle)
      || this._debugUnknownHandle(handle)
}

proto.resolveHandles = function resolve() {
  this._opts.handles.forEach(this._addHandle, this);
  return this._resolvedHandles;
}

var defaultOpts = {
  source: true, highlight: true, attachHandle: false
}

var exports = module.exports = function coreActiveHandles(opts) {
  opts = opts || defaultOpts;
  opts.handles = opts.handles || process._getActiveHandles();
  if (!opts.handles.length) return [];
  return new HandleResolver(opts).resolveHandles();
}

exports.handleType = handleType;
exports.defaultOpts = defaultOpts;
exports.locationString = locationString;

},{"_function_origin":undefined,"util":undefined}],11:[function(require,module,exports){
'use strict';

var getFunctionLocation = require('_function_origin')
  , constants           = process.binding('constants')
  , format              = require('util').format

var requestType = {
    unknown         : { flag: 0x0,        name : 'unknown' }
  , AccessContext   : { flag: 0x1,        name: 'access',     api: 'fs.access(path[, mode], callback)' }
  , ExistsContext   : { flag: 0x2,        name: 'exists',     api: 'fs.exists(path, callback) -- DEPRECATED' }
  , ReadFileContext : { flag: 0x4,        name: 'readFile',   api: 'fs.readFile(filename[, options], callback)' }
  , CloseContext    : { flag: 0x8,        name: 'close',      api: 'fs.close(fd, callback)' }
  , OpenContext     : { flag: 0x10,       name: 'open',       api: 'fs.open(path, flags[, mode], callback)' }
  , ReadContext     : { flag: 0x20,       name: 'read',       api: 'fs.read(fd, buffer, offset, length, position, callback)' }
  , WriteContext    : { flag: 0x40,       name: 'write',      api: 'fs.write(fd, buffer, offset, length[, position], callback)' }
  , TruncateContext : { flag: 0x80,       name: 'truncate',   api: 'fs.truncate(path, len, callback)' }
  , FTruncateContext: { flag: 0x100,      name: 'ftruncate',  api: 'fs.ftruncate(fd, len, callback)' }
  , RmdirContext    : { flag: 0x200,      name: 'rmdir',      api: 'fs.rmdir(path, callback)' }
  , FDatasyncContext: { flag: 0x400,      name: 'fdatasync',  api: 'fs.fdatasync(fd, callback)' }
  , FSyncContext    : { flag: 0x800,      name: 'fsync',      api: 'fs.fsync(fd, callback)' }
  , MkdirContext    : { flag: 0x1000,     name: 'mkdir',      api: 'fs.mkdir(path[, mode], callback)' }
  , ReaddirContext  : { flag: 0x2000,     name: 'readdir',    api: 'fs.readdir(path, callback)' }
  , FStatContext    : { flag: 0x4000,     name: 'fstat',      api: 'fs.fstat(fd, callback)' }
  , LStatContext    : { flag: 0x8000,     name: 'lstat',      api: 'fs.lstat(path, callback)' }
  , StatContext     : { flag: 0x10000,    name: 'stat',       api: 'fs.stat(path, callback)' }
  , ReadlinkContext : { flag: 0x20000,    name: 'readlink',   api: 'fs.readlink(path, callback)' }
  , SymlinkContext  : { flag: 0x40000,    name: 'symlink',    api: 'fs.symlink(destination, path[, type], callback)' }
  , LinkContext     : { flag: 0x80000,    name: 'link',       api: 'fs.link(srcpath, dstpath, callback)' }
  , UnlinkContext   : { flag: 0x100000,   name: 'unlink',     api: 'fs.unlink(path, callback)' }
  , FChmodContext   : { flag: 0x200000,   name: 'fchmod',     api: 'fs.fchmod(fd, mode, callback)' }
  , ChmodContext    : { flag: 0x400000,   name: 'chmod',      api: 'fs.chmod(path, mode, callback)' }
  , FChownContext   : { flag: 0x800000,   name: 'fchown',     api: 'fs.fchown(fd, uid, gid, callback)' }
  , ChownContext    : { flag: 0x1000000,  name: 'chown',      api: 'fs.chown(path, uid, gid, callback)' }
  , UtimesContext   : { flag: 0x2000000,  name: 'utimes',     api: 'fs.utimes(path, atime, mtime, callback)' }
  , FUtimesContext  : { flag: 0x4000000,  name: 'futimes',    api: 'fs.futimes(fd, atime, mtime, callback)' }
};

var accessModes = (function getAccessModes() {
  function addMode(acc, k) {
    acc[constants[k]] = k;
    return acc;
  }
  return [ 'R_OK', 'W_OK', 'X_OK', 'F_OK' ].reduce(addMode, {});
})();

var permissions = [
    '---' // 00  no permissions
  , 'x--' // 01  execute
  , '-w-' // 02  write
  , '-wx' // 03  write & execute
  , 'r--' // 04  read
  , 'r-x' // 05  read & execute
  , 'rw-' // 06  read & write
  , 'rwx' // 07  read, write, & execute
];

function humanizePermission(mode) {
  var owner      = mode >>> 6;         // shift lower 6 bits off to get owner
  var ownerGroup = mode >>> 3;         // shift off user
  var group      = ownerGroup & 0o007; // mask out owner to get group
  var user       = mode & 0o007;       // mask out group and owner to get user

  return '-' + permissions[owner] + permissions[group] + permissions[user] +
        ' (' + mode.toString(8) + ')';
}

function shallowClone(obj) {
  var keys = Object.keys(obj);

  function copy(acc, k) {
    acc[k] = obj[k];
    return acc;
  }
  return keys.reduce(copy, {});
}

function getPrototypeName(obj) {
  if (typeof obj !== 'object' || obj === null) return '!not-an-object!';
  return Object.getPrototypeOf(obj).constructor.name;
}

function isPrototype(obj, s) {
  if (typeof obj !== 'object' || obj === null) return false;
  return getPrototypeName(obj) === s;
}

function locationString(loc) {
  return loc
    ? format('%s:%d:%d', loc.file, loc.line, loc.column)
    : 'Unknown location';
}

function functionNameInfo(fn, location) {
  var anonymous, name;
  if (!fn.name || !fn.name.length) {
    name = location.inferredName && location.inferredName.length
        ? location.inferredName
        : '__unknown_function_name__';

    anonymous = true;
  } else {
    name = fn.name;
    anonymous = false;
  }

  return { name: name, anonymous: anonymous };
}

function humanizeMode(type, mode) {
  switch (type) {
    case 'AccessContext':
      return accessModes[mode] + ' (' + mode + ')';
    case 'OpenContext':
    case 'MkdirContext':
    case 'FChmodContext':
    case 'ChmodContext':
      return humanizePermission(mode)
    default:
      return 'UNKNOWN MODE';
  }
}

function RequestResolver(opts) {
  if (!(this instanceof RequestResolver)) return new RequestResolver(opts);

  this._opts = opts;
  this._resolvedRequests = [];
}

var proto = RequestResolver.prototype;

proto._functionInfo = function _functionInfo(fn, req) {
  if (typeof fn !== 'function') return {};

  var location = getFunctionLocation(fn)
  // v8 zero bases lines
  if (location) location.line++;

  var nameInfo = functionNameInfo(fn, location);

  // handle anonymous functions and try to figure out a meaningful function name
  var ret = {
      fn          : fn
    , name        : nameInfo.name
    , location    : location
    , anonymous   : nameInfo.anonymous
  }

  // otherwise add source
  // if any feature specified in opts needs it
  if (this._opts.highlight || this._opts.source) ret.source = fn.toString();
  if (this._opts.attachRequest) ret.request = req;

  return ret;
}

proto._addFunctionInfo = function _addFunctionInfo(fn, req, type) {
  var info = this._functionInfo(fn, req);
  info.type = type || requestType.unknown;
  info.details = {};
  this._resolvedRequests.push(info);
  return info;
}

function inspect(obj, depth) {
  console.error(require('util').inspect(obj, false, depth || 5, true));
}
proto._resolveRequest = function _resolveRequest(req) {
  // In rare cases (https://github.com/nodesource/nsolid-node/issues/110) we get requests
  // that aren't related to an fs method. The linked issue exposed a case where we had a TCP handle
  // and no context.
  // TODO(thlorenz) The below will ignore those requests, but we'll need to investigate further
  // why we are getting those requests.
  if (typeof req.context !== 'object') return true;

  var ctx = shallowClone(req.context);  // clone to avoid changing actual context so other tools aren't affected
  var type = getPrototypeName(req.context);

  var info = this._addFunctionInfo(ctx.callback, req, requestType[type]);

  if (typeof ctx.mode !== 'undefined') ctx.mode = humanizeMode(type, ctx.mode);
  info.details = ctx;
  return true;
}

proto._addRequest = function _addRequest(req) {
  return this._resolveRequest(req);
}

proto.resolveRequests = function resolveRequests() {
  this._opts.requests.forEach(this._addRequest, this);
  return this._resolvedRequests;
}

var defaultOpts = {
  source: true, highlight: true, attachRequest: false
}

var exports = module.exports = function coreActiveRequests(opts) {
  opts = opts || defaultOpts;
  opts.requests = opts.requests || process._getActiveRequests();
  if (!opts.requests.length) return [];
  return new RequestResolver(opts).resolveRequests();
}

exports.requestType = requestType;
exports.defaultOpts = defaultOpts;
exports.locationString = locationString;

},{"_function_origin":undefined,"util":undefined}],12:[function(require,module,exports){
var os = require('os')

var stats = require('./lib/stats')

var wrapper = function(stat_type) {

  return function(pid, options, cb) {

    if(typeof options == 'function') {
      cb = options
      options = {}
    }

    return stats[stat_type](pid, options, cb)
  }
}

var pusage = {
  darwin: wrapper('ps'),
  sunos: wrapper('ps'),
  freebsd: wrapper('ps'),
  win: wrapper('win'),
  linux: wrapper('proc'),
  aix: wrapper('ps'),
  unsupported: function(pid, options, cb) {
    cb = typeof options == 'function' ? options : cb

    cb(new Error(os.platform()+' is not supported yet, please fire an issue (https://github.com/soyuka/pidusage)'))
  }
}

var platform = os.platform();
platform = platform.match(/^win/) ? 'win' : platform; //nor is windows a winner...
platform = pusage[platform] ? platform : 'unsupported';

exports.stat = function() {
  pusage[platform].apply(stats, [].slice.call(arguments));
};

exports.unmonitor = function(pid) {
  delete stats.history[pid];
};

exports._history = stats.history;

},{"./lib/stats":14,"os":undefined}],13:[function(require,module,exports){
var os = require('os')
  , exec = require('child_process').exec

module.exports = {
  getconf: function(keyword, options, next) {

    if(typeof options == 'function') {
      next = options
      options = { default: '' }
    }

    exec('getconf '+keyword, function(error, stdout, stderr) {
      if(error !== null) {
        console.error('Error while getting '+keyword, error)
        return next(null, options.default)
      }

      stdout = parseInt(stdout)

      if(!isNaN(stdout)) {
        return next(null, stdout)
      }

      return next(null, options.default)
    })
  },
  cpu: function(next) {
    var self = this

    self.getconf('CLK_TCK', {default: 100}, function(err, clock_tick) {
      self.getconf('PAGESIZE', {default: 4096}, function(err, pagesize) {

        next(null, {
          clock_tick: clock_tick,
          uptime: os.uptime(),
          pagesize: pagesize
        })

       })
    })
  }
}

},{"child_process":undefined,"os":undefined}],14:[function(require,module,exports){
var os = require('os')
  , fs = require('fs')
  , p = require('path')
  , exec = require('child_process').exec
  , spawn = require('child_process').spawn
  , helpers = require('./helpers')

var stats = {
  history: {},
  cpu: null, //used to store cpu informations
  proc: function(pid, options, done) {
        var self = this

    if(this.cpu !== null) {
      fs.readFile('/proc/uptime', 'utf8', function(err, uptime) {
        if(err) {
          return done(err, null)
        } else if(uptime === undefined) {
          console.error("We couldn't find uptime from /proc/uptime")
          self.cpu.uptime = os.uptime()
        } else {
          self.cpu.uptime = uptime.split(' ')[0]
        }

        return self.proc_calc(pid, options, done)
      })
    } else {
      helpers.cpu(function(err, cpu) {
        self.cpu = cpu
        return self.proc_calc(pid, options, done)
      })
    }
  },
  proc_calc: function(pid, options, done) {
    var history = this.history[pid] ? this.history[pid] : {}, cpu = this.cpu
      , self = this

    //Arguments to path.join must be strings
    fs.readFile(p.join('/proc', ''+pid, 'stat'), 'utf8', function(err, infos) {

      if(err)
        return done(err, null)

      //https://github.com/arunoda/node-usage/commit/a6ca74ecb8dd452c3c00ed2bde93294d7bb75aa8
      //preventing process space in name by removing values before last ) (pid (name) ...)
      var index = infos.lastIndexOf(')')
      infos = infos.substr(index + 2).split(' ')

      //according to http://man7.org/linux/man-pages/man5/proc.5.html (index 0 based - 2)
      //In kernels before Linux 2.6, start was expressed in jiffies. Since Linux 2.6, the value is expressed in clock ticks
      var stat = {
          utime: parseFloat(infos[11]),
          stime: parseFloat(infos[12]),
          cutime: parseFloat(infos[13]),
          cstime: parseFloat(infos[14]),
          start: parseFloat(infos[19]) / cpu.clock_tick,
          rss: parseFloat(infos[21])
      }

      //http://stackoverflow.com/questions/16726779/total-cpu-usage-of-an-application-from-proc-pid-stat/16736599#16736599

      var childrens = options.childrens ? stat.cutime + stat.cstime : 0;
      var total = 0;

      if(history.utime) {
        total = (stat.stime - history.stime) + (stat.utime - history.utime) + childrens
      } else {
        total = stat.stime + stat.utime + childrens
      }

      total = total / cpu.clock_tick

        //time elapsed between calls
        var seconds = history.uptime !== undefined ? cpu.uptime - history.uptime : stat.start - cpu.uptime
        seconds = Math.abs(seconds)
        seconds = seconds === 0 ? 0.1 : seconds //we sure can't divide through 0

      self.history[pid] = stat
      self.history[pid].seconds = seconds
      self.history[pid].uptime = cpu.uptime

      return done(null, {
        cpu: (total / seconds) * 100,
        memory: stat.rss * cpu.pagesize
      })
    })
  },
  /**
   * Get pid informations through ps command
   * @param  {int}   pid
   * @return  {Function} done (err, stat)
   * on os x skip headers with pcpu=,rss=
   * on linux it could be --no-header
   * on solaris 11 can't figure out a way to do this properly so...
   */
  ps: function(pid, options, done) {

    var cmd = 'ps -o pcpu,rss -p '

    if(os.platform() == 'aix')
      cmd = 'ps -o pcpu,rssize -p ' //this one could work on other platforms

    exec(cmd + pid, function(error, stdout, stderr) {
      if(error) {
        return done(error)
      }

      stdout = stdout.split(os.EOL)[1]
      stdout = stdout.replace(/^\s+/, '').replace(/\s\s+/g, ' ').split(' ')

      return done(null, {
        cpu: parseFloat(stdout[0].replace(',', '.')),
        memory: parseFloat(stdout[1]) * 1024
      })
    })
  },
  /**
   * This is really in a beta stage
   */
  win: function(pid, options, done) {
      //  http://social.msdn.microsoft.com/Forums/en-US/469ec6b7-4727-4773-9dc7-6e3de40e87b8/cpu-usage-in-for-each-active-process-how-is-this-best-determined-and-implemented-in-an?forum=csharplanguage
      //var wmic = spawn('wmic' ['PROCESS', pid, 'get workingsetsize,usermodetime,kernelmodetime']);
      var wmic = spawn('wmic', ['PROCESS', pid, 'get', 'workingsetsize,usermodetime,kernelmodetime'],
          {detached: true}
      );
      var stdout = '';
      wmic.stdout.on('data', function (data) {
          stdout += data;
      });
      wmic.stderr.on('data', function (data) {
          console.warn(data);
      });

      wmic.on('close', function () {
          stdout = stdout.split(os.EOL)[1];
          stdout = stdout.replace(/\s\s+/g, ' ').split(' ');
          var stats = {
              kernelmodetime: parseFloat(stdout[0]),
              usermodetime: parseFloat(stdout[1]),
              workingsetsize: parseFloat(stdout[2])
          };
          // according to http://technet.microsoft.com/en-us/library/ee176718.aspx
          var total = (stats.usermodetime + stats.kernelmodetime) / 10000000; //seconds
          return done(null, {cpu: total, memory: stats.workingsetsize});
      });

      wmic.on('error', function (err) {
          console.error("wmic spawn error: " + err);
          return done(err);
      });

  }
}

module.exports = stats;

},{"./helpers":13,"child_process":undefined,"fs":undefined,"os":undefined,"path":undefined}],15:[function(require,module,exports){
"use strict"

const setImmediate = global.setImmediate;

const commands = require('./commands/index.js')
const etcd = require('./etcd.js')
const generateId = require("./id")
const listen_options = require("./listen_options").listen_options;
const debug = require("util").debuglog("nsolid")
const process = global.process;
const binding = process._linkedBinding('nsolid_agent');
const forEach = Function.call.bind(Array.prototype.forEach);
const keys = Function.call.bind(Object.keys, Object);
const bind = Function.call.bind(Function.bind);

const stringify = JSON.stringify.bind(JSON);
const errorPrototype = Error.prototype;
const deferred = []

var initialized = false

module.exports = agent
const id = agent.id = generateId()
const name = agent.appName = process.env.NSOLID_APPNAME || "nsolid-default"
function on(event, handler) {
  if (typeof event !== 'string') {
    throw new Error('event needs to be a string not typeof ' + typeof event);
  }
  if (typeof handler !== 'function') {
    throw new Error('event needs to be a function not typeof ' + typeof event);
  }
  binding.registerJSCommand('/'+event, handler, false);
  return agent;
}

agent.on = function (event, handler) {
  return on(event, handler);
}
function agent(HUB, SOCKET, agent_cb) {
  var local = null;
  var remote = null;
  if (SOCKET) {
    const local_addr = listen_options(SOCKET)
    local = {
      port: local_addr.port,
      address: local_addr.host
    };
  }
  if (HUB) {
    const remote_addr = listen_options(HUB)
    remote = {
      port: remote_addr.port,
      address: remote_addr.host
    };
  }
  if (!local && !remote) {
    return agent;
  }

  // We want the agent to be a singleton
  if (initialized) {
    debug("noop -- nsolid already initialized")
    return agent
  }
  initialized = true

  const etcdOptions = etcd.getOptions(name, id);

  binding.setup({
     name: name,
     id: id,
     etcdTTL: 10,
     etcdPathname: etcdOptions.etcdPathname,
     etcdPayloadTemplate: etcdOptions.etcdPayloadTemplate,
     emit: function emit() {
       arguments[0] = slice(String(arguments[0]), 1);
       const listeners = call(ee_listeners, null, arguments[0]); 
       if (!listeners || !listeners.length) {
         arguments[1].throw("NSolid Agent: no command handler installed for `"+arguments[0]+"`");
         return;
       }
       apply(ee_emit, null, arguments);
     },
     errorPrototype: errorPrototype, 
     stringify: stringify,
     local: local,
     remote: remote,
     // C++ will call this when it is ready
     notify: function (settings) {
       // used for tests
       debug("nsolid initialized on port " + settings.local.port)
       if (typeof agent_cb === 'function') {
         agent_cb(settings);
       }
     }
  });  

  // Register commands
  debug("registering default commands")
  forEach(keys(commands), function (k) {
    const cmd = commands[k];
    if (!cmd) {
      return;
    }
    if (typeof cmd.init === 'function') {
      cmd.init(agent);
    }
    if (typeof cmd.attach === 'function') {
      deferred[deferred.length] = bind(cmd.attach, null, agent);
    }
  });
  binding.lockdown();
  debug("nsolid initializing")
  setImmediate(function startAgent() {
    binding.spawn();
  })
  return agent;
}

if (process.env.NSOLID_HUB || process.env.NSOLID_SOCKET) {
  const HUB = process.env.NSOLID_HUB;
  const SOCKET = process.env.NSOLID_SOCKET;
  agent(HUB, SOCKET);
}

},{"./commands/index.js":2,"./etcd.js":7,"./id":8,"./listen_options":9,"util":undefined}]},{},[15])(15)
});