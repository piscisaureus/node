'use strict';

const schema_topLevel = {
  blacklist: 'object',
  process: 'object'
};

const schema_blacklist = {
  modules: 'object',
  bindings: 'object'
};

const schema_process = {
  zeroFillAllocations: 'boolean'
};

function stringifyKeys(obj) {
  return Object.keys(obj).join(', ');
}

function getPrototypeName(obj) {
  return Object.getPrototypeOf(obj).constructor.name;
}

function strictIsObject(obj) {
  return typeof obj === 'object' && obj !== null &&
         getPrototypeName(obj) === 'Object';
}

function describeType(obj) {
  const type = typeof obj;
  return type === 'object' ? getPrototypeName(obj) : type;
}

function toLookup(arr) {
  function hashify(acc, k) {
    acc[k] = true;
    return acc;
  }
  return arr.reduce(hashify, { __keys: arr });
}

function deepFreeze(o) {
  Object.freeze(o);

  function freeze(prop) {
    if (o.hasOwnProperty(prop)
    && o[prop] !== null
    && (typeof o[prop] === 'object' || typeof o[prop] === 'function')
    && !Object.isFrozen(o[prop])) {
      deepFreeze(o[prop]);
    }
  }

  Object.getOwnPropertyNames(o).forEach(freeze);
  return o;
}

function isComment(key) {
  return /^\W*\/\//.test(key);
}

/**
 * Validates the policies against the nsolid policies schema.
 * It is subject to evolve over time and thus this validator
 * needs to be updated each time.
 *
 * ### Valid Policies Example
 *
 * ```
 *  {
 *    "blacklist": {
 *      "modules": {
 *        "child_process": 2,
 *        "fs": {
 *          "write": 3,
 *          "read": 1
 *        },
 *        "path": 0
 *      }
 *      "bindings": {
 *        "stream_wrap": 2
 *      },
 *    },
 *    "process": {
 *      "zeroFillAllocations": true
 *    }
 *  }
 * ```
 *
 * @name PoliciesValidator
 * @param {Object} policies policies to validate
 * @param {Array.<string>} bindings list of known core bindings
 * @param {Array.<string>} modules list of known core modules
 */
function PoliciesValidator(policies, bindings, modules) {
  if (!(this instanceof PoliciesValidator))
    return new PoliciesValidator(policies, bindings, modules);

  this._policies = policies;
  this._builtins = {
    bindings: toLookup(bindings),
    modules:  toLookup(modules),
  };
  this._errors = [];
}

var proto = PoliciesValidator.prototype;

proto.validate = function validate() {
  this._validateTopLevel();

  // if we encounter a top level error let's wait til that gets fixed
  // before we dig into each of the top level properties
  if (this._hasError()) return this._result();

  this._validateBlacklist();
  this._validateProcess();

  return this._result();
};

proto._result = function _result() {
  return { hasError: this._hasError(), errors: this._errors };
};

proto._hasError = function _hasError() {
  return !!this._errors.length;
};

proto._addError = function _addError(msg) {
  this._errors.push(msg);
};

proto._validateTopLevel = function validateTopLevel(k) {
  Object.keys(this._policies).forEach(this._validateTopLevelKey, this);
};

proto._validateTopLevelKey = function validateTopLevelKey(k) {
  if (isComment(k)) return;
  if (typeof schema_topLevel[k] === 'undefined') {
    return this._addError('Unknown top level property "' + k + '"\n' +
                          'Known properties are ' +
                          stringifyKeys(schema_topLevel) + '.');
  }

  if (typeof this._policies[k] !== schema_topLevel[k]) {
    this._addError('Top level property "' + k + '" has invalid type "' +
                    describeType(this._policies[k]) + '". ' +
                    'Expected type "' + schema_topLevel[k] + '".');
  }
};

proto._validateBlacklist = function _validateBlacklist() {
  // At this point we already determined that the blacklist property
  // is an object if it was present.
  const bl = this._policies.blacklist;
  if (typeof bl === 'undefined') return;

  Object.keys(bl).forEach(this._validateBlacklistKey, this);
};


proto._validateBlacklistKey = function _validateBlacklistKey(k) {
  if (isComment(k)) return;
  if (typeof schema_blacklist[k] === 'undefined') {
    return this._addError('Unknown blacklist property "' + k + '"\n' +
                          'Known properties are ' +
                          stringifyKeys(schema_blacklist) + '.');
  }

  if (typeof this._policies.blacklist[k] !== schema_blacklist[k]) {
    return this._addError('Blacklist property "' + k + '" has invalid type "' +
                          describeType(this._policies.blacklist[k]) + '". ' +
                          'Expected type "' + schema_blacklist[k] + '".');
  }

  this._validateBlacklistValues(k);
};

proto._validateBlacklistValues =
  function _validateBlacklistValues(blacklistName) {
    // Check that each modules/bindings blacklist value is either an object or
    // a severity setting between 0-3.
    // The validity of the blacklisted specific methods is determined
    // during _module_wrap init.
    var blacklist = this._policies.blacklist[blacklistName];
    if (typeof blacklist !== 'undefined') {
      var keys = Object.keys(blacklist);
      var builtins = this._builtins[blacklistName];
      for (var i = 0; i < keys.length; i++) {
        if (isComment(keys[i])) continue;
        this._validateBlacklistedExists(keys[i], builtins, blacklistName);
        this._validateBlacklistValue(keys[i], blacklist, blacklistName);
      }
    }
  };

proto._validateBlacklistedExists =
  function _validateBlacklistedExists(k, builtins, blacklistName) {
    if (!builtins[k]) {
      this._addError('One of the blacklisted ' + blacklistName +
                    ' "' + k + '" does not exist.\n' +
                    'Only "' + builtins.__keys.join(', ') + '" are known.');

    }
  };

proto.__validateBlacklistValue =
  function __validateBlacklistValue(val, propname, objectOk) {
    const expected = objectOk
      ? 'Expected an Object or an Integer from 0 to 3.'
      : 'Expected an Integer from 0 to 3.';

    if (typeof val !== 'number') {
      return this._addError('Blacklist property "' + propname  +
                            '" has invalid type "' + describeType(val) + '". ' +
                            expected);
    }
    if ((val >> 0) !== val) {
      return this._addError('Blacklist property "' + propname +
                            '" is a floating point Number "' + val + '". ' +
                            expected);
    }
    if (0 > val || val > 3) {
      return this._addError('Blacklist property "' + propname +
                            '": ' + val + ' is out of bounds. ' +
                            expected);
    }
  };

proto._validateBlacklistValue =
  function _validateBlacklistValue(k, blacklist, blacklistName) {
    const val = blacklist[k];
    const propname =  blacklistName + '.' + k;

    // blacklisted entire module/binding
    if (!strictIsObject(val))
      return this.__validateBlacklistValue(val, propname, true);

    // blacklisted specific methods on module/binding
    var key;
    const keys = Object.keys(val);
    for (var i = 0; i < keys.length; i++) {
      key = keys[i];
      if (isComment(key)) continue;
      this.__validateBlacklistValue(val[key], propname + '.' + key, false);
    }
  };

proto._validateProcess = function _validateProcess() {
  const proc = this._policies.process;
  if (typeof proc === 'undefined') return;

  Object.keys(proc).forEach(this._validateProcessKey, this);
};

proto._validateProcessKey = function _validateProcessKey(k) {
  if (isComment(k)) return;
  if (typeof schema_process[k] === 'undefined') {
    return this._addError('Unknown process property "' + k + '"\n' +
                          'Known properties are ' +
                          stringifyKeys(schema_process) + '.');
  }
  if (typeof this._policies.process[k] !== schema_process[k]) {
    return this._addError('process property "' + k + '" has invalid type "' +
                          describeType(this._policies.process[k]) + '". ' +
                          'Expected type "' + schema_process[k] + '".');
  }
  // So far only boolean properties are allowed on process, so once
  // the keys and value types are correct we are done.
};

/*
 * Freezes the given policies recursively, making all properties
 * readonly.
 *
 * Then runs them through the policies validator.
 *
 * For now we perform super simple validation, just checking for
 * high level properties and that they have the correct type.
 * If we need to have a full blown JSON schema validator consider
 * geraintluff/tv4.
 *
 * We do check if modules/bindings that are blacklisted do exist,
 * but since we cannot inspect their methods we don't check if
 * individual methods that are blacklisted on the module/binding exist.
 * That step is performed when _module_wrap is initialized since
 * then we know what methods exist on the given module/binding.
 *
 * The fact that a part of the blacklist validation happens a bit later is ok
 * as all policy rules that are essential during node's initialization are
 * already available and validated until then.
 *
 * @name validate
 * @param {Object} policies policies to validate
 * @param {Array.<string>} bindings list of known core bindings
 * @param {Array.<string>} modules list of known core modules
 */
function validate(policies, bindings, modules) {
  try {
    deepFreeze(policies);
    const validator = new PoliciesValidator(policies, bindings, modules);
    return validator.validate();
  } catch (err) {
    return { hasError: true, errors: [ err.toString() ] };
  }
}

(function getValidate() { return validate; })();

// Allow validator tests to require this module.
// module.exports will be undefined when this script is sourced.
if (typeof module === 'object' && typeof module.exports === 'object')
  module.exports = validate;
