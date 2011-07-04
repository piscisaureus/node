
global.errno = undefined;

var binding = process.binding('cares_wrap');

function d(a,b) { console.log(errno, a, b) };

var req = binding.queryA('www.google.com', d);
console.log(req, errno);


var req = binding.queryA('localhost', d);
console.log(req, errno);



var req = binding.queryAaaa('ipv6.test-ipv6.com', d);
console.log(req, errno);


var req = binding.getHostByAddr('72.14.225.72', d);
console.log(req, errno);



var req = binding.getHostByAddr('2001:470:1:18::2', d);
console.log(req, errno);



var req = binding.getHostByAddr('fdsfdh', d);
console.log(req, errno);


var req = binding.queryMx('gmail.com', d);
console.log(req, errno);


var req = binding.queryNs('byte.nl', d);
console.log(req, errno);


var req = binding.querySrv('_jabber._tcp.google.com', d);
console.log(req, errno);


var req = binding.queryCname('www.google.com', d);
console.log(req, errno);




