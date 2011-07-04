
var dns=require('dns');
function d(a,b,c) { console.log(a,b,c); };
dns.lookup("www.nu.nl", 6, d);