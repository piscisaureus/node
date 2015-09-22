'use strict';

function KnownModules(require_, binding_) {
  if (!(this instanceof KnownModules)) {
    return new KnownModules(require_, binding_);
  }
  this._keysCache = {};
  // this require bypasses our own black listing mechanism
  this._require = require_;
  //binding_ is process.binding before it was wrapped
  this._coreModuleNames = Object.keys(binding_('natives'));
}

var proto = KnownModules.prototype;

proto.keysFor = function keysFor(id) {
  // We assume that we'll never be asked for a core module that doesn't exist.
  // We rely on the policy validator to ensure that for us.
  if (this._keysCache[id]) return this._keysCache[id];

  // Won't have to worry about file access here as we'll only get here
  // for core modules (see above) which are bundled with the process.
  this._keysCache[id] = Object.keys(this._require(id));
  return this._keysCache[id];
};

proto.coreModuleNames = function coreModuleNames() {
  // Hard coding all module names may not be the best idea as they may change.
  // We could determine dynamically via `require` if a core module exists.
  // However we need to know them to validate policies.
  return this._coreModuleNames;
};

module.exports = KnownModules;
