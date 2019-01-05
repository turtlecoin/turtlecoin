var Lbl = require('line-by-line')
var lr = new Lbl('checkpoints.csv')
var util = require('util')

lr.on('line', (line) => {
  var p = line.split(',')
  p[0] = parseInt(p[0])
  if (p[0] % 5000 === 0) {
    console.log(util.format('{%s, "%s"},', p[0].toString().padStart(8), p[1]))
  }
})
