
var binding = process.binding('net');

var accept = binding.accept;
var socket = binding.socket;
var connect = binding.connect;
var bind = binding.bind;
var listen = binding.listen;
var read = binding.read;
var write = binding.write;

console.log('socket');
var fd = socket("tcp4");

console.log('bind');
bind(fd, 3456);

console.log('listen');
listen(fd);

console.log('accepting...');
accept(fd, onAccept);

console.log('socket 2');
var fd2 = socket("tcp4");

console.log('bind 2');
bind(fd2, 0);

console.log('connecting...');
connect(fd2, 3456, '1.2.3.4', onConnect);

function onConnect(err) {
  console.log(['Connect result', err, fd])
}

function onAccept(err, fd) {
  console.log(['Accept result', err, fd]);
}