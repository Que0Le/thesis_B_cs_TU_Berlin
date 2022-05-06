ssh -i /home/que/.ssh/id_rsa que@192.168.1.12 "cd ~/Desktop/xdp-tutorial/eBPF_measure && make clean && make" &&
rsync -avIL -e "ssh -i /home/que/.ssh/id_rsa" que@192.168.1.12:~/Desktop/xdp-tutorial/eBPF_measure/* /home/que/Desktop/mymodule/eBPF_measure &&
echo "Running ebpf_measure_user ..."
sudo ./ebpf_measure_user -d ens33 --filename ebpf_measure_kern.o