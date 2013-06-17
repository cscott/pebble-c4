#!/usr/bin/env node
var fs = require('fs');

var c4 = fs.readFileSync('../resources/src/c4defs.tsv', 'utf8');
var lines = c4.split('\n');
console.log(lines.length);
console.log(Math.max.apply(Math, lines.map(function(s) { return s.length; })));
