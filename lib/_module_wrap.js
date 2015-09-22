'use strict';

const policies = process._linkedBinding('policies');

// set in exports.init
var knownModules, knownBindings;
var blacklist;

function requiredFromCore(id, stack) {
  var lines = stack.split('\n');
  var requireRegex = /at require \(module.js/;
  //  relying on `/` to indicate that module is required from a user module
  //  if we support windows we need to improve on that most likely
  var notCoreRegex = /at\s\S+\s\(\//;

  for (var i = 0; i < lines.length; i++) {
    if (requireRegex.test(lines[i]) &&
        lines[i + 1] && notCoreRegex.test(lines[i + 1])) {
      log('debug',
          `'${ id }' is blacklisted and ` +
          `not required from core and therefore will be wrapped.`);
      return false;
    }
  }

  log('debug',
      `'${ id }' is blacklisted but ` +
      `required from core and therefore not wrapped.`);
  return true;
}

function canRead(fs, p) {
  try {
    fs.accessSync(p, 'r');
    return true;
  } catch (e) {
    return false;
  }
}

function blacklistInfo(hash, id) {
  if (typeof hash === 'undefined' ||
      typeof hash[id] === 'undefined') {
    return { blacklisted: false, severity: 0 };
  }
  const severity = hash[id];
  return { blacklisted: !!severity, severity: severity };
}

function moduleBlacklistInfo(id) {
  return blacklistInfo(blacklist.modules, id);
}

function bindingBlacklistInfo(id) {
  return blacklistInfo(blacklist.bindings, id);
}

/**
 * Invoked whenever NativeModule.getSource is called.
 *
 * Returns either the provided source or our custom version if the 'id' was
 * blacklisted.
 * The wrapped version prints a warning immediately indicating
 * from where the module is required and that it is blacklisted.
 * Once any function of it is accessed it throws an Error.
 *
 * @param {string} id the module request id, i.e. 'fs'
 * @param {string} source the source of the module as provided by core
 * @return {string}
 *    either the original source or our version with function wrappers
 */
exports.getSource = function getSource(id, source) {
  log('debug', `Handling '${ id }'`);

  const blacklistInfo = moduleBlacklistInfo(id);
  if (!blacklistInfo.blacklisted) return source;

  var stack = new Error().stack;
  if (requiredFromCore(id, stack)) return source;

  if (blacklistInfo.severity === 1) {
    log('error', 'Module "' + id + '" is required');
    log('error', resolveModuleParent(stack));
    log('error', 'But it was disabled in the policies.');
    log('warn', 'Severity level 1: continuing execution, ' +
                'please review code and policy settings.');
    return source;
  }

  log('debug', `Wrapping requested module '${ id }'`);

  var replacement = moduleReplacementFor(knownModules.keysFor(id),
                                         blacklistInfo.severity);

  if (!replacement) {
    // this should no longer happen as we are able to resolve
    // all core modules and validate policies to ensure no
    // non-core modules are attempted to be blacklisted
    throw new Error(`Module '${ id }' is black listed, ` +
                    `but no replacement was found.`);
  }

  return `'use strict'\r\n ${ replacement }`;
};


/**
 * Invoked whenever NativeModule.getCached is called.
 *
 * Return `null` if a module is blacklisted in order to simulate
 * it not being found inside cache.
 * This will then trigger a `getSource` invocation allowing
 * us to wrap the module.
 *
 * @param {string} id the module request id, i.e. 'fs'
 * @param {Object} cached the currently cached module for the id
 * @return {Object} the cached module if it is not blacklisted, otherwise `null`
 */
exports.getCached = function getCached(id, cached) {
  if (!cached) return null;
  const blacklistInfo = moduleBlacklistInfo(id);
  if (!blacklistInfo.blacklisted) return cached;

  var stack = new Error().stack;
  if (requiredFromCore(id, stack)) return cached;

  // force getSource which will trigger the function above
  log('debug',
      'Returning `null` simulating not found ' +
      'in cache in order to force `getSource`.');
  return null;
};


/**
 * Initializes _module_wrap module.
 *
 * Prepares module and binding replacements.
 * Wraps the `process.binding` function in order to allow us
 * to wrap blacklisted bindings.
 *
 * @param {Object} knownModules_
 *    known core modules service which resolves keys of methods for each core
 *    module that may be replaced via policies and for which we can generate
 *    wrappers
 * @param {Object} knownBindings_
 *    known core bindings service which resolves keys of methods for each core
 *    binding which may be replaced via policies and for which we can generate
 *    wrappers
 */
exports.init = function init(knownModules_, knownBindings_) {
  knownModules = knownModules_;
  knownBindings = knownBindings_;
  const config = policies.getPolicies();
  blacklist = (config && config.blacklist) || {};

  wrapProcessBinding();
};

/*
 * Utility functions used by both module and binding replacements
 */

// Copy paste from ./lib/util.js and simplified
// (we cant require anything)
function colorize(str, col) {
  var colors = {
    white: [37, 39],
    grey: [90, 39],
    black: [30, 39],
    blue: [34, 39],
    cyan: [36, 39],
    green: [32, 39],
    magenta: [35, 39],
    red: [31, 39],
    yellow: [33, 39]
  };

  return col ?
      '\u001b[' + colors[col][0] + 'm' + str +
      '\u001b[' + colors[col][1] + 'm' :
      str;
}

function captureStack() {
  var stack = {};
  Error.captureStackTrace(stack, captureStack);
  return stack.stack;
}

function log(type, str) {
  var col;
  if (type === 'debug' && !process.env.NSOLID_MODULE_WRAP_DEBUG) return;
  switch (type) {
    case 'info' : col = 'green'; break;
    case 'warn' : col = 'yellow'; break;
    case 'error' : col = 'red'; break;
    case 'debug' : col = 'grey'; break;
    default : col = undefined;
  }
  process._rawDebug(
      colorize('nsolid', 'grey') + ' ' + colorize(type, col) + ' ' + str);
}

/*
 * process.binding wrap (no code generated)
 */
function resolveBindingCaller(stack) {
  // module.parent is not set
  var lines = stack.split('\n');
  var requireRegex = /at process.bindingWrap/;

  for (var i = 0; i < lines.length; i++) {
    if (requireRegex.test(lines[i]) && lines[i + 1])
      return lines[i + 1].trim();
  }
  return 'at an unknown location';
}

function bindingRequestedFromCore(id, stack) {
  var lines = stack.split('\n');
  var notCoreRegex = /at\s\S+\s\(\//;
  log('debug', stack);

  // Examples:
  // not core
  //  at process.bindingWrap [as binding] (_module_wrap.js:163:17)
  //  at Object.<anonymous>
  //    (/Volumes/d/dev/ns/nsolid/nsolid-node/nsolid-ext/test/policies/fixtures/
  //          binding-process_wrap.WriteWrap.js:3:23)
  // not core:
  //  at process.bindingWrap [as binding] (_module_wrap.js:163:17)
  //  at requestBinding
  //    (/Volumes/d/dev/ns/nsolid/nsolid-node/nsolid-ext/test/policies/fixtures/
  //        binding-process_wrap-infn.WriteWrap.js:4:25)
  // core:
  //  at process.bindingWrap [as binding] (_module_wrap.js:163:17)
  //  at spawnSync (child_process.js:1277:26)
  //
  //  relying on `/` to indicate that module is resolved from a user module
  //  if we support windows we need to improve on that most likely

  var core = lines.length > 2 && !notCoreRegex.test(lines[2]);

  if (core)
    log('debug',
        `Binding '${ id }' is blacklisted ` +
        `but requested from core and therefore not wrapped.`);
  else
    log('debug',
        `Binding '${ id }' is blacklisted ` +
        `and not requested from core and therefore will be wrapped.`);

  return core;
}

function wrapProcessBinding() {
  // only wrap bindings any of them were blacklisted
  if (!blacklist ||
      !blacklist.bindings ||
      !Object.keys(blacklist.bindings).length) return;

  var binding_ = process.binding;

  function bindingWrap(id) {
    log('debug', 'Handling binding "' + id + '".');
    const blacklistInfo = bindingBlacklistInfo(id);
    if (!blacklistInfo.blacklisted) return binding_(id);

    var stack = captureStack();
    if (bindingRequestedFromCore(id, stack)) return binding_(id);

    log('error',
        'Binding "' + id + '" is requested via a process.binding call');
    log('error', resolveBindingCaller(stack));
    log('error', 'But it was disabled in the policies.');

    if (blacklistInfo.severity === 1) {
      log('warn', 'Severity level 1: continuing execution, ' +
                  'please review code and policy settings.');
      return binding_(id);
    }

    var replacement = bindingReplacementFor(id,
                                            knownBindings.keysFor(id),
                                            blacklistInfo.severity);
    if (!replacement) {
      // this should no longer happen as we are able to resolve
      // all core bindings and validate policies to ensure no
      // non-core bindings are attempted to be blacklisted
      throw new Error('Binding "' + id +
                      '" is black listed, but no replacement was found.');
    }
    return replacement;
  }

  Object.defineProperty(process, 'binding', {
    enumerable: true,
    configurable: false,
    writable: false,
    value: bindingWrap
  });
}

/*********************************************
 *
 * Code that generates the module replacements.
 *
 * These are designed to print a warning on initialization and
 * handle one of its methods being called depending on the severity:
 *
 * - 1: print a warning
 * - 2: print a warning and throw an error
 * - 3: print a warning and abort the process
 *
 *********************************************/

function resolveModuleParent(stack) {
  // module.parent is not set
  var lines = stack.split('\n');
  var requireRegex = /at require \(module.js/;

  for (var i = 0; i < lines.length; i++) {
    if (requireRegex.test(lines[i]) && lines[i + 1])
      return lines[i + 1].trim();
  }
  return 'at an unknown location';
}

function shutdownProcess() {
  // Try different ways to shut down the process
  // and hope that one succeeds. The first one should
  // work unless someone mucked with the function.
  try {
    // SIGKILL bypasses any process.on('exit') handlers.
    process.kill(process.pid, 'SIGKILL');
  } catch(e) {}
  try {
    process.reallyExit();
  } catch(e) {}
  process.abort();
}


const moduleHeader = `;
${ colorize.toString() };
${ log.toString() };

${ shutdownProcess.toString() };

${ resolveModuleParent.toString() };

${ captureStack.toString() };

var stack = captureStack();
log('error', 'Module "' + module.id + '" is required');
log('error', resolveModuleParent(stack));
log('error', 'But it was disabled in the policies.');
`;

const warnHandler = `
function handleDisabledMethodCall() {
  log('error', '"' + module.id + '" has been marked forbidden ' +
               'by the policies flag, it should not be in use.');
}
`;

const throwHandler = `
function handleDisabledMethodCall() {
  throw new Error('"' + module.id + '" has been disabled ' +
                  'by the policies flag, terminating execution.');
}
`;

const exitHandler = `
function handleDisabledMethodCall() {
  log('error', '"' + module.id + '" has been disabled by the policies flag, ' +
               'terminating execution.');
  log('error', 'Severity level 3: shutting down.');
  shutdownProcess();
}
`;

function toGetter(k) {
  return `get ${ k }() { handleDisabledMethodCall() }`;
}

function generateExports(moduleExports, severity) {
  var getters = moduleExports.map(toGetter).join(',\n    ');
  var s = `;
  module.exports = {
    ${ getters }
  };`;
  return s;
}

function moduleReplacementFor(moduleExports, severity) {
  // TODO (thlorenz) warn handler is never used until we handle
  // severity 1 more granular. At this point only a warning
  // is printed and the actual core module returned.
  const disabledMethodHandler =
      severity === 1 ? warnHandler
    : severity === 2 ? throwHandler
    : exitHandler;

  const s = `;
  ${ moduleHeader };
  ${ disabledMethodHandler }
  ${ generateExports(moduleExports) }`;
  return s;
}

function warnWrap(id, prop) {
  return function warn() {
    log('error', 'Binding "' + id +
                  '" has been marked forbidden, therefore "' + prop +
                  '" should not be in use.');
  };
}

function throwWrap(id, prop) {
  return function throwError() {
    throw new Error('Binding "' + id +
                    '" has been disabled by the policies flag, using "' + prop +
                    '" on it is not allowed, terminating execution.');
  };
}

function exitWrap(id, prop) {
  return function warnAndExit() {
    log('error', 'Binding "' + id +
                 '" has been disabled by the policies flag, using "' + prop +
                 '" on it is not allowed, terminating execution.');
    log('error', 'Severity level 3: shutting down.');
    shutdownProcess();
  };
}

function bindingReplacementFor(id, properties, severity) {
  var b = {};

  // TODO (thlorenz) warn wrap is never used until we handle
  // severity 1 more granular (see moduleReplacmentFor).
  const disabledMethodWrap =
      severity === 1 ? warnWrap
    : severity === 2 ? throwWrap
    : exitWrap;


  for (var i = 0; i < properties.length; i++) {
    Object.defineProperty(b, properties[i], {
      enumerable: true,
      configurable: false,
      writable: false,
      value: disabledMethodWrap(id, properties[i])
    });
  }
  return b;
}
