
var test_utf8_new = process.test_utf8_new;
var test_utf8_old_hint = process.test_utf8_old_hint;
var test_utf8_old_nohint = process.test_utf8_old_nohint;

function benchmark_all() {
  var size = 8;
  var iterations = 2500000;
  while (true) {
    var log = Math.floor(log10(iterations)),
        factor = Math.pow(10, log),
        mantissa = Math.round(iterations / factor),
        round_iterations = mantissa * factor;
    benchmark_shapes(size, round_iterations);
    if (size <= 16) {
      iterations /= 2;
    } else if (size <= 64) {
      iterations /= 4
    } else {
      iterations /= 8;
    }
    size *= 8;
    if (iterations < 1) return;
  }
}

function benchmark_shapes(size, iterations) {
  var shapes = ["left_tailed", "right_tailed", "tree", "flat"];
  for (var i = 0; i < shapes.length; i++) {
    for (var j = 0; j <= 2; j++) {
      benchmark(size, shapes[i], j, iterations);
    }
  }
}

function benchmark(size, shape, unicode, iterations) {
  var string = shape_generators[shape](size, unicode),
      i, start, end;
  
  process.stdout.write("size: " + string.length + ", shape: " + shape);
  process.stdout.write(", content: " + ["ascii", "single nonascii", "mixed"][unicode]);
  process.stdout.write(", iterations: " + iterations);
  
  process.stdout.write("\nnew: ");
  start = (new Date()).getTime();
  for (i = iterations - 1; i >= 0; i--)
    test_utf8_new(string);
  end = (new Date()).getTime();
  process.stdout.write((end - start) / 1000 + " s");
  
  process.stdout.write("\told_nohint: ");
  start = (new Date()).getTime();
  for (i = iterations - 1; i >= 0; i--)
    test_utf8_old_nohint(string);
  end = (new Date()).getTime();
  process.stdout.write((end - start) / 1000 + " s");
  
  process.stdout.write("\told_hint: ");
  start = (new Date()).getTime();
  for (i = iterations - 1; i >= 0; i--)
    test_utf8_old_hint(string);
  end = (new Date()).getTime();
  process.stdout.write((end - start) / 1000 + " s");
  
  process.stdout.write("\n\n");
}

/* 
 * Helpers 
 */

function log10(n) {
  return Math.log(n) / Math.log(10);
}

/*
 * Shape generators
 */

function part(unicode) {
  if (unicode) return "ü1234567";
  else return "12345678";
}

var shape_generators = {
  left_tailed: function(size, unicode) {
    var s = part(unicode);
    while (s.length < size) {
      s = part(unicode > 1) + s;
    }
    return s;
  },

  right_tailed: function(size, unicode) {
    var s = part(unicode);
    while (s.length < size) {
      s = s + part(unicode > 1);
    }
    return s;
  },

  tree: function(size, unicode) {
    s = part(unicode > 1);
    while (s.length < size) {
      s = s + s;
    }
    var s = s + part(unicode);
    return s;
  },

  flat: function(size, unicode) {
    return new Buffer(shape_generators.tree(size, unicode)).toString();
  }
};

benchmark_all();
