import math
import sys
import matplotlib
import matplotlib.pyplot as plt

def readFile(pathToFile):
  data = {'labels': [], 'data': []}
  with open(str(pathToFile)) as f:
    lines = f.read().splitlines()
    for line in lines:
      lineAsList = line.split()
      benchmark = str(lineAsList[0])
      speedup = float(lineAsList[1])
      data['labels'].append(benchmark)
      data['data'].append(speedup)
    f.close()

  return data

def getArgs():
  args = {}
  numOfArgs = 1

  lenArgv = len(sys.argv)
  if (lenArgv < numOfArgs + 1):
    print('USAGE: $ python ' + sys.argv[0] + ' path/to/file.txt')
    sys.exit(1)

  args['pathToFile'] = sys.argv[1]

  return args

args = getArgs()
data = readFile(args['pathToFile'])

labels = data['labels']

fig = plt.figure()
ax = fig.add_subplot(111)

fontSize = 12

matplotlib.rcParams.update({'font.size': fontSize})
ax.set_ylabel('Speedup', fontsize = fontSize)

barWidth = 0.4
xTicks = range(len(labels))
ax.bar(xTicks, data['data'], barWidth, color = 'black')

ymin = 0
ymax = math.ceil(max(data['data']))
ystep = 1

plt.xticks(xTicks, labels, fontsize = fontSize, rotation = 30, ha = 'right')
plt.yticks(range(ymin, ymax + 1, ystep), fontsize = fontSize)

xmin = xTicks[0]
xmax = xTicks[-1]

ax.set_ylim(ymin = ymin, ymax = ymax + 0.5)

# Lines
#ax.plot([xmin, xmax], [ymax, ymax], '--', color = 'k', linewidth = 1.5)
#geomeanx = ((x[-1] - x[-2])/2.0) + x[-2]
#ax.plot([geomeanx, geomeanx], [ymin, ymax], '--', color = 'k', linewidth = 1.5)

# Annotations
#fontSizeAnnotation = 11
#ax.annotate('Maximum speedup', fontsize=fontSizeAnnotation, xy=(0.1, 29.5), color = 'black', bbox = dict(ec='white', fc = 'white', alpha = 1))

ax.yaxis.grid(True, color = 'gray', ls = '--')
ax.set_axisbelow(True)

ax.tick_params(axis = 'x', direction = 'out', top = False)

#ax.legend(fontsize = fontSize, fancybox = False, framealpha = 1, ncol = len(legend), loc = 'upper left') # handletextpad = 0.5, columnspacing = 1.1, bbox_to_anchor = (1.0, 1.03), bbox_transform = ax.transAxes

matplotlib.rcParams['pdf.fonttype'] = 42
matplotlib.rcParams['ps.fonttype'] = 42

#ax.set_aspect(0.02)

plt.tight_layout()
plt.savefig('./barplot.pdf', format = 'pdf')
#plt.show()
