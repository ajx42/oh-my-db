#Used for parsing general performance logs and graphing results
import sys
import pandas as pd
import matplotlib.pyplot as plt
import os

def convert_to_seconds(time_str):
	h, m, s = map(int, time_str.split(':'))
	return h * 3600 + m * 60 + s


def parseLoadLog(inFile, nodeName):
	timeList = []
	load1mList = []
	load5mList = []
	load15mList = []
	startingTime = None

	with open(inFile) as f:
		for l in f.readlines():
			l = l.replace(',', '')
			time = convert_to_seconds(l.split(' ')[1])

			if startingTime == None:
				startingTime = time

			timeList.append(time -startingTime)

			if "average" not in l:
				load1mList.append(float(l.split(' ')[2]))
				load5mList.append(float(l.split(' ')[3]))
				load15mList.append(float(l.split(' ')[4]))
			else:
				load1mList.append(float(l.split(' ')[3]))
				load5mList.append(float(l.split(' ')[4]))
				load15mList.append(float(0))

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

	startingTime = None

	with open(inFile) as f:
		for l in f.readlines():
			time = convert_to_seconds(l.split(' ')[1])

			if startingTime == None:
				startingTime = time

			timeList.append(time - startingTime)
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

def average_data(data, avgRange):
	dataAvg = []
	for i in range(len(data)):
        # Calculate the indices of the surrounding values
		start_idx = max(0, i - avgRange)
		end_idx = min(len(data) - 1, i + avgRange)
		num_vals = end_idx - start_idx + 1
				
		# Calculate the average of the surrounding values
		avg = sum(data[start_idx:end_idx+1]) / num_vals
		dataAvg.append(avg)
	
	return dataAvg



def parseNetLog(inFile, nodeName):
	timeList = []
	net1InList = []
	net1OutList = []
	net2InList = []
	net2OutList = []

	startingTime = None	

	with open(inFile) as f:
		for l in f.readlines():
			l = " ".join(l.split()) 
			if "Time" in l or "HH:MM:SS" in l or not l:
				continue
			

			time = convert_to_seconds(l.split(' ')[0])

			if startingTime == None:
				startingTime = time
			#if time < 62000:
			#	continue
			timeList.append(time -startingTime)

			net1InList.append(float(l.split(' ')[1]))
			net1OutList.append(float(l.split(' ')[2]))
			net2InList.append(float(l.split(' ')[3]))
			net2OutList.append(float(l.split(' ')[4]))

	net2InList = average_data(net2InList, 10)
	net2OutList = average_data(net2OutList, 10)

	return pd.DataFrame(
		dict(
			node=nodeName,
			time=timeList,
			net1In=net1InList,
			net1Out=net1OutList,
			net2In=net2InList,
			net2Out=net2OutList,
		)
	)

def graphData(data, groupField, xField, yField, xLabel, yLabel, title=""):
	font = {'family' : 'normal',
        'weight' : 'normal',
        'size'   : 14}
	plt.rc('font', **font)

	grouped_mem = data.groupby(groupField)

	fig, ax = plt.subplots()

	for group_name, group_df in grouped_mem:
		ax.plot(group_df[xField], group_df[yField], label=group_name)

	ax.legend()
	ax.set_xlabel(xLabel)
	ax.set_ylabel(yLabel)
	ax.set_title(title)
	plt.show()

def graphNet(data):
	grouped_mem = data.groupby("node")

	fig, ax = plt.subplots()

	for group_name, group_df in grouped_mem:
		ax.plot(group_df["time"], group_df["net1In"], label=group_name)
		ax.plot(group_df["time"], group_df["net1Out"], label=group_name)

	ax.legend()
	ax.set_xlabel("Time (s)")
	ax.set_ylabel("Network Usage KB/s")
	ax.set_title("Network")
	plt.show()



def main(inFile):

	NextNodeID = 0
	NodeNameMapping = {}
	data = {}
	loadData = []
	memData = []
	netData = []

	#Fill dict with test results
	for root, dirs, files in os.walk(inFile):
		for filename in files:
			nodeName = root.split('/')[-1].split('@')[-1].split('.')[0]

			if nodeName not in data.keys():
				data[nodeName] = {}
				NodeNameMapping[nodeName] = "Node " + str(NextNodeID)
				NextNodeID += 1

			nodeName = NodeNameMapping[nodeName]

			if filename == "load.log.dat":
				loadData.append(parseLoadLog(os.path.join(root, filename), nodeName))

			elif filename == "mem.log.dat":
				memData.append(parseMemLog(os.path.join(root, filename), nodeName))

			elif filename == "net.log.dat":
				netData.append(parseNetLog(os.path.join(root, filename), nodeName))

	combined_load = pd.concat(loadData, ignore_index=True)
	combined_mem = pd.concat(memData, ignore_index=True)
	combined_net = pd.concat(netData, ignore_index=True)

	print(NodeNameMapping)
	graphData(combined_mem, "node", "time", "used", "Time (s)", "Memory Usage (MB)", "10 ms Leader: Node Memory Usage (MB)")
	graphData(combined_mem, "node", "time", "shared", "Time (s)", "Shared Memory Usage (MB)", "10 ms Leader: Node Memory Usage (MB)")
	graphData(combined_mem, "node", "time", "buffCache", "Time (s)", "Buffer/Cache Memory Usage (MB)", "10 ms Leader: Node Memory Usage Buffer/Cache (MB)")
	graphData(combined_load, "node", "time", "load1m", "Time (s)", "Unix System Load (5 Minute Avg)", "10 ms Leader:Node System Load")
	graphData(combined_net, "node", "time", "net2In", "Time (s)", "Network Usage (KB/s)", "10 ms Leader: Incoming Network Traffic")
	graphData(combined_net, "node", "time", "net2Out", "Time (s)", "Network Usage (KB/s)", "10 ms Leader: Outgoing Network Traffic")


if __name__ == "__main__":
	main(sys.argv[1])
