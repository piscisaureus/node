
var binding = process.binding('net');

var accept = binding.accept;
var socket = binding.socket;
var connect = binding.connect;
var bind = binding.bind;
var listen = binding.listen;
var read = binding.read;
var write = binding.write;

// Read/write buffer size
var BSIZE = 16384;
var PORT = 3457;

// Stats
var start = +(new Date());
var bytes_read = 0, bytes_written = 0, bytes_written_pending = 0;

// Create a server
var fd_s = socket("tcp4");
bind(fd_s, PORT);
listen(fd_s);
console.log("server listening");

// Show stats every second
setInterval(displayStats, 1000);

// Test with n socket pairs
for (var i = 0; i < 1000; i++)
  testPair(fd_s);

function testPair(fd_s) {
  var fd1, fd2;
  var accepted = false, connected = false;

  fd1 = socket("tcp4");
  bind(fd1, 0);
  accept(fd_s, onAccept);
  connect(fd1, PORT, undefined, onConnect);

  function onConnect(err) {
    if (err) throw err;
    connected = true;
    onEstablished();
  }

  function onAccept(err, fd) {
    if (err) throw err;
    fd2 = fd;
    accepted = true;
    onEstablished();  
  }

  function onEstablished() {
    if (!(connected && accepted))
      return;
    
    console.log('Socket pair created');
    
    var writeBuffer = new Buffer(BSIZE);
    var readBuffer = new Buffer(BSIZE);
    
    var pending = 0;
    
    function doWrite() {
      if (pending > BSIZE * 2) return;
    
      pending += BSIZE;
      bytes_written_pending += BSIZE;
      write(fd1, writeBuffer, 0, BSIZE, function doneWrite(err, bytes) {
        if (err) throw err;

        bytes_written += bytes;

        doWrite();
      });
    }

    function doRead() {
      read(fd2, readBuffer, 0, BSIZE, function doneRead(err, bytes) {
        if (err) throw err;

        pending -= bytes;
        bytes_read += bytes;
        
        doRead();
        doWrite();
      });
    }

    doWrite(fd1);
    doRead(fd2);
  }
}

function displayStats() {
  var MBIT = 1000 * 1000 / 8;
  var elapsed = ((new Date()) - start) / 1000;
  var in_mbit = Math.round(bytes_read / elapsed / MBIT);
  var out_mbit = Math.round(bytes_written / elapsed / MBIT);
  var outp_mbit = Math.round(bytes_written_pending / elapsed / MBIT);
  
  console.log(["in: ", in_mbit, " mbit/s, out: ", out_mbit, " mbit/s, out+pending: ", outp_mbit, " mbit/s"].join(''));
}

