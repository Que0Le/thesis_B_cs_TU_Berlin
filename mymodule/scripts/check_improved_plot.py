import os.path
import sys
import matplotlib.pyplot as plt
import numpy as np
import statistics

log_path = "/home/que/Desktop/mymodule/logs/"
path_km = log_path + "KM_kern.txt"
path_km_user = log_path + "KM_user.txt"
path_km_server_linuxsocket = log_path + "KM_socket_server.txt"
# #
# path_xdp_kern = log_path + "XDP_kern.txt"
# path_xdp_us = log_path + "XDP_user.txt"

path_ebpf_server_linuxsocket = log_path + "EBPF_socket_server.txt"
path_ebpf_kern = log_path + "EBPF_kern.txt"
path_ebpf_us = log_path + "EBPF_user.txt"


full_log_paths = [
    path_km,
    path_km_user,
    path_km_server_linuxsocket,
    path_ebpf_server_linuxsocket,
    path_ebpf_kern,
    path_ebpf_us
]

for path in full_log_paths:
    if not os.path.isfile(path):
        print("file: " + str(path) + " not existed! Exit now ...")
        exit()

to_usec = 1
v = 1000/to_usec

thresholds = [
    [0, 1],
    [1, 2],
    [2, 3],
    [3, 4],
    [4, 5],
    [5, 6],
    [6, 7],
    [7, 8],
    [8, 9],
    [9, 10],
    [10, 11],
    [11, 13],
    [13, 16],
    [16, 19],
    [19, 23],
    [23, 27],
    [27, 32],
    [32, 40],
    [40, 50],
    [50, 60],
    [60, 70],
    [70, 80],
    [80, 90],
    [90, 100],
    [100, 150],
    [150, 200],
    [200, 250],
    [250, 300],
    [300, 400],
    [400, 500],
    [500, 800],
    [800, 1000],
    [1000, 100000],
]
labels = []
for i in range(0, len(thresholds)):
    # if i==0:
    #     l = str("-maxsize") + '-' + str(thresholds[i][1])
    # elif i==(len(thresholds)-1):
    #     l = str(thresholds[i][0]) + '-' + str("maxsize")
    # else:
    #     l = str(thresholds[i][0]) + '-' + str(thresholds[i][1])
    l = str(thresholds[i][0]) + '-' + str(thresholds[i][1])
    labels.append(l)



km_log = []
up_log = []
km_s_log = []
ebpf_log = []
usebpf_log = []
ebpf_s_log = []

""" Open logs and load data """
with open(path_km) as km_log_f:
    for line in km_log_f:
        km_log.append(int(line))
with open(path_km_user) as up_log_f:
    for line in up_log_f:
        up_log.append(int(line))
with open(path_km_server_linuxsocket) as km_s_log_f:
    for line in km_s_log_f:
        km_s_log.append(int(line))
# #
with open(path_ebpf_kern) as ebpf_log_f:
    for line in ebpf_log_f:
        ebpf_log.append(int(line))
with open(path_ebpf_us) as usebpf_log_f:
    for line in usebpf_log_f:
        usebpf_log.append(int(line))
with open(path_ebpf_server_linuxsocket) as ebpf_s_log_f:
    for line in ebpf_s_log_f:
        ebpf_s_log.append(int(line))

""" Check lengths """
if len(km_log)!=len(up_log) or \
    len(km_log)!=len(km_s_log) or \
        len(ebpf_log)!=len(usebpf_log) or \
            len(ebpf_log)!=len(ebpf_s_log) or \
                len(km_s_log)!=len(ebpf_s_log):
    print("Files have diff length. Abort!")
    exit()

""" Check data correctness """
logs = [km_log, up_log, km_s_log, ebpf_log, usebpf_log, ebpf_s_log]
logs_label = ["km_log", "up_log", "km_s_log", "ebpf_log", "usebpf_log", "ebpf_s_log"]
logs_zeroed = [0] * len(logs)
###
# diffs_up_km = [0] * len(km_log)
# diffs_s_km = [0] * len(km_log)
# diffs_s_up = [0] * len(km_log)
count_diffs_up_km = {}
count_diffs_s_km = {}
count_diffs_s_up = {}

# diffs_usebpf_ebpf = [0] * len(km_log)
# diffs_s_ebpf = [0] * len(km_log)
# diffs_s_usebpf = [0] * len(km_log)
count_diffs_usebpf_ebpf = {}
count_diffs_s_ebpf = {}
count_diffs_s_usebpf = {}

count_diffs = [
    count_diffs_up_km, count_diffs_s_km, count_diffs_s_up, 
    count_diffs_usebpf_ebpf, count_diffs_s_ebpf, count_diffs_s_usebpf
]
# diffs = [diffs_up_km, diffs_s_km, diffs_s_up, diffs_usebpf_ebpf, diffs_s_ebpf, diffs_s_usebpf]
diffs_label = [
    "diffs_up_km", "diffs_s_km", "diffs_s_up", 
    "diffs_usebpf_ebpf", "diffs_s_ebpf", "diffs_s_usebpf"
]
diffs_neg = [0] * len(diffs_label)

""" 
km_log = []
up_log = []
km_s_log = []
ebpf_log = []
usebpf_log = []
ebpf_s_log = []
"""
to_subtract = [
    (1,0), # up_log - km_log
    (2,0), # km_s_log - km_log
    (2,1), # km_s_log - up_log
    (4,3), # usebpf_log - ebpf_log
    (5,3), # ebpf_s_log - ebpf_log
    (5,4), # ebpf_s_log - usebpf_log
]
""" Calculate the diffs """
for i in range(0, len(km_log)):
    ### Cal diffs
    for pair_th in range(0, len(to_subtract)):
        pair = to_subtract[pair_th]
        ### Check zero
        z = 0
        if logs[pair[0]][i]==0:
            logs_zeroed[pair[0]] = logs_zeroed[pair[0]] + 1
            z+=1
        if logs[pair[1]][i]==0:
            logs_zeroed[pair[1]] = logs_zeroed[pair[1]] + 1
            z+=1
        if z!=0:
            continue    # No need to calculate diff because one of the measurement is zero
        d = logs[pair[0]][i] - logs[pair[1]][i]
        # Check neg
        if d<0:
            # print(f"{diffs_label[pair_th]} {d}")
            diffs_neg[pair_th] += 1
        # Add to counter
        g = count_diffs[pair_th].get(d)
        if g:
            count_diffs[pair_th][d] = g+1
        else:
            count_diffs[pair_th][d] = 1
# Because each zero value is counted twice, we modify them to correct value
logs_zeroed = list(map(lambda v: int(v/2), logs_zeroed))

print("#################")
print("Zeroed: ")
for i in range(0, len(diffs_label)):
    if logs_zeroed[i] != 0:
        print(f"{str(logs_label[i])}: {str(logs_zeroed[i])}")
print("#################")
print("Negatived: ")
for i in range(0, len(diffs_label)):
    if diffs_neg[i] != 0:
        print(f"{str(diffs_label[i])}: {str(diffs_neg[i])}")
print("#################")
print("Max-Min: ")
for i in range(0, len(diffs_label)):
    print(f"{str(diffs_label[i])}: {str(max(count_diffs[i].keys()))}-{str(min(count_diffs[i].keys()))}")
print("#################")

# count_diffs   diffs_label
for i in range(0, len(diffs_label)):
    sumary = 0
    length = 0
    for key, value in count_diffs[i].items():
        # if key<0 or key>100000:
        #     continue
        sumary += key*value
        length += value
    print(f"Avg {diffs_label[i]} = {sumary/length}")

diff_up_km = [0] * len(thresholds)
diff_s_km = [0] * len(thresholds)
diff_s_up = [0] * len(thresholds)

diff_usebpf_ebpf = [0] * len(thresholds)
diff_s_ebpf = [0] * len(thresholds)
diff_s_usebpf = [0] * len(thresholds)

diffs_count_in_threshold_ranges = [
    diff_up_km, diff_s_km, diff_s_up,
    diff_usebpf_ebpf, diff_s_ebpf, diff_s_usebpf
]

""" Export unexpected values to file to evaluate later """
with open("out_range.txt", 'w') as f:
    for i_cd in range(0, len(count_diffs)):
        to_pop = []
        for item in count_diffs[i_cd].items():
            diff = int(item[0]/to_usec)
            for i_thres in range(0, len(thresholds)):
                if (diff >= v*thresholds[i_thres][0]) and (diff < v*thresholds[i_thres][1]):
                    # If value in threshold range, we increase the counter and add this item to pop list
                    diffs_count_in_threshold_ranges[i_cd][i_thres] += item[1]
                    to_pop.append(item[0])
                    break
        [count_diffs[i_cd].pop(x) for x in to_pop]
        # After that, this dictionatry should only contain value that are out of threshold (neg, too big, ...)
        f.write("--------------------------------\n")
        f.write(str(diffs_label[i_cd]) + "\n")
        for key in sorted(count_diffs[i_cd]):
            f.write(f"{key}: {count_diffs[i_cd][key]}\n")

""" Plotting """
x = np.arange(len(labels))  # the label locations
width = 0.11  # the width of the bars

fig, ax = plt.subplots()
rects1 = ax.bar(
    x - 3*width, diff_up_km, width,
    label='diff_up_km:')
rects2 = ax.bar(
    x - 2*width, diff_usebpf_ebpf, width,
    label='diff_usebpf_ebpf:')
rects3 = ax.bar(
    x - 1*width, diff_s_km, width, 
    label='diff_s_km')
#
rects4 = ax.bar(
    x + 1*width, diff_s_ebpf, width, 
    label='diff_s_ebpf')
# rects5 = ax.bar(
#     x + 2*width, diff_s_up, width, 
#     label='diff_s_up')
# rects6 = ax.bar(
#     x + 3*width, diff_s_usebpf, width, 
#     label='diff_s_usebpf')
# Add some text for labels, title and custom x-axis tick labels, etc.
if v==1000:
    ax.set_ylabel('Number of packets')
    ax.set_xlabel('Diff in nsec (*1000)')
    ax.set_title('Number of packets received sorted in latency. Total: ' + str(len(km_log)))
elif v==1:
    ax.set_ylabel('Number of packets')
    ax.set_xlabel('Diff in usec')
    ax.set_title('Number of packets received sorted in latency. Total: ' + str(len(km_log)))

ax.set_xticks(x)
ax.set_xticklabels(labels)
ax.legend()

ax.bar_label(rects1, padding=3)
ax.bar_label(rects2, padding=3)
ax.bar_label(rects3, padding=3)
ax.bar_label(rects4, padding=3)
# ax.bar_label(rects5, padding=3)
# ax.bar_label(rects6, padding=3)
fig.set_size_inches(18.5, 10.5)
fig.tight_layout()

plt.xticks(rotation='vertical')
plt.yticks(rotation='vertical')
plt.show()
