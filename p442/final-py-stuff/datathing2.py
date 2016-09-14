

import numpy as np
import matplotlib.pyplot as plt

data = np.loadtxt('testfile.txt',dtype='|S4', converters={0:lambda s:int(s,16)})
hex2milliamps = ((3.0 / 4096.0) * 100.0)
y = data[:,0]
values = map(int, y)
results = hex2milliamps*np.array(values)
x = data[:,1]
counter=0
n=[]

while counter < len(results):
	n.append(counter)
	counter+=1

plt.plot(n, results, 'ro')
plt.xlabel('Time Interval')
plt.ylabel('Milliamps')
plt.xlim(0.0, counter+1)
plt.ylim(0.0, 50.)
plt.show()

'''
with open("test.txt") as f:
    data = f.read()



data = data.split('\n')
print(data)
x = [row.split(' ')[0] for row in data]
print(x)
y = [row.split(' ')[1] for row in data]
print(y)

fig = plt.figure()

ax1 = fig.add_subplot(111)

ax1.set_title("Plot title...")    
ax1.set_xlabel('your x label..')
ax1.set_ylabel('your y label...')

ax1.plot(x,y, c='r', label='the data')

leg = ax1.legend()

plt.show()

import matplotlib.pyplot as plt

x = []
y = []

readFile = open ('test.txt', 'r')

sepFile = readFile.read().split('\n')
readFile.close()

for plotPair in sepFile:
    xAndY = plotPair.split(' ')
    x.append(int(float(xAndY[0])))
    y.append(int(float(xAndY[1])))

print x
print y
'''