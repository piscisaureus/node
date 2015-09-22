'use strict';

function KnownBindings(binding_) {
  if (!(this instanceof KnownBindings)) return new KnownBindings(binding_);
  this._keysCache = {};
  // process.binding before it was wrapped
  this._binding = binding_;
  this._knownBindingNames = Object.keys(binding_('builtins'));
}

var proto = KnownBindings.prototype;

proto.keysFor = function keysFor(id) {
  // We assume that we'll never be asked for a core binding that doesn't exist.
  // We rely on the policy validator to ensure that for us.
  if (this._keysCache[id]) return this._keysCache[id];

  // Won't have to worry about file access here as we'll only get here
  // for core bindings (see above) which are bundled with the process.
  this._keysCache[id] = Object.keys(this._binding(id));
  return this._keysCache[id];
};

proto.coreBindingeNames = function coreBindingNames() {
  return this._coreBindingNames;
};

module.exports = KnownBindings;
