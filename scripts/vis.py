import sys
import pandas as pd
import matplotlib.pyplot as plt
import os


class NetData:
	time = 0

	net1In = 0
	net1Out = 0

	net2In = 0
	net2Out = 0


class LoadData:
	time = 0

	load1m = 0
	load5m = 0
	load15m = 0


class MemData:
	time = 0

	total = 0
	used = 0
	free = 0
	shared = 0
	buffCache = 0
	available = 0


def RunTimeGraph(df):
	font = {'family': 'normal',
         'weight': 'normal',
         'size': 14}
	plt.rc('font', **font)
	plt.bar('jobcount', 'time', data=df, align='center', alpha=0.5)
	plt.xlabel("Thread Count")
	plt.ylabel("Build Time (s)")
	plt.title("leveldb: Parallel Build Time")
	plt.show()


def get_sec(time_str):
    """Get seconds from time."""
    m, s = time_str.split(':')
    return int(m) * 60 + float(s)


def extractData(f):
	jobCountList = []
	timeList = []

	state = 0

	for l in f.readlines():

		if "Command being timed" in l:
			JobCount = int(l.split()[-1].replace('"', ''))
			jobCountList.append(JobCount)

		if "wall clock" in l:
			timeList.append(get_sec(l.split()[-1]))

	return pd.DataFrame(
		dict(
			jobcount=jobCountList,
			time=timeList
		)
	)


def convert_to_seconds(time_str):
    h, m, s = map(int, time_str.split(':'))
    return h * 3600 + m * 60 + s


def parseLoadLog(inFile, nodeName):
	timeList = []
	load1mList = []
	load5mList = []
	load15mList = []
	with open(inFile) as f:
		for l in f.readlines():
			l = l.replace(',', '')
			timeList.append(convert_to_seconds(l.split(' ')[1]))
			load1mList.append(float(l.split(' ')[2]))
			load5mList.append(float(l.split(' ')[3]))
			load15mList.append(float(l.split(' ')[4]))

	return pd.DataFrame(
		dict(
			node=nodeName,
			time=timeList,
			load1m=load1mList,
			load5m=load5mList,
			load15m=load15mList,
		)
	)

def parseMemLog(inFile, nodeName):
	timeList = []
	totalList = []
	usedList = []
	freeList = []
	sharedList = []
	buffCacheList = []
	availableList = []

	with open(inFile) as f:
		for l in f.readlines():
			timeList.append(convert_to_seconds(l.split(' ')[1]))
			totalList.append(int(l.split(' ')[2]))
			usedList.append(int(l.split(' ')[3]))
			freeList.append(int(l.split(' ')[4]))
			sharedList.append(int(l.split(' ')[5]))
			buffCacheList.append(int(l.split(' ')[6]))
			availableList.append(int(l.split(' ')[7]))

	return pd.DataFrame(
		dict(
			node=nodeName,
			time=timeList,
			total=totalList,
			used=usedList,
			free=freeList,
			shared=sharedList,
			buffCache=buffCacheList,
			available=availableList,
		)
	)


def graphData(data, groupField, xField, yField, xLabel, yLabel, title=""):
	grouped_mem = data.groupby(groupField)

	fig, ax = plt.subplots()

	for group_name, group_df in grouped_mem:
		ax.plot(group_df[xField], group_df[yField], label=group_name)

	ax.legend()
	ax.set_xlabel(xLabel)
	ax.set_ylabel(yLabel)
	ax.set_title(title)
	plt.show()


def main(inFile):

	data = {}
	loadData = []
	memData = []

	#Fill dict with test results
	for root, dirs, files in os.walk(inFile):
		for filename in files:
			#print(os.path.join(root, filename))
			nodeName = root.split('/')[-1].split('@')[-1].split('.')[0]

			if nodeName not in data.keys():
				data[nodeName] = {}

			if filename == "load.log.dat":
				loadData.append(parseLoadLog(os.path.join(root, filename), nodeName))

			elif filename == "mem.log.dat":
				memData.append(parseMemLog(os.path.join(root, filename), nodeName))

	combined_load = pd.concat(loadData, ignore_index=True)
	combined_mem = pd.concat(memData, ignore_index=True)

	graphData(combined_mem, "node", "time", "used", "Time (s)", "Memory Usage (MB)", "Node Memory Usage (MB)")
	graphData(combined_load, "node", "time", "load1m", "Time (s)", "Unix System Load (1 Minute Avg)", "Node System Load")


if __name__ == "__main__":
	main(sys.argv[1])
