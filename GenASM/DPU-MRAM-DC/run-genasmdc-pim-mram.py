import argparse
import math
import os

ap = argparse.ArgumentParser()
ap.add_argument("-i", "--input", type=str, required=True,
                help="Input read pairs file path")
ap.add_argument("-o", "--output", type=str,
                help="Output alignment file path", default="./out")
ap.add_argument("-l", "--read_length", required=True,
                type=int, help="Read Length")
ap.add_argument("-e", "--error", type=float,
                help="Percentage error per read length (or provide max edit distance)")
ap.add_argument("-n", "--number_reads", type=int, required=True,
                help="Number of read pairs to be aligned")
ap.add_argument("-m", "--match_cost", type=int, default=0,
                help="Cost of characters match")
ap.add_argument("-x", "--mismatch_cost", type=int, default=3,
                help="Cost of characters mismatch")
ap.add_argument("-g", "--gap_opening", type=int, default=4,
                help="Cost of opening a new gap")
ap.add_argument("-a", "--gap_extending", type=int,
                default=1, help="Cost of extending gap")
ap.add_argument("-k", "--max_edit", type=int,
                help="max edit distance operations (optional or provide percentage error)")
ap.add_argument("-t", "--nr_of_tasklets", type=int,
                help="NR_TASKLETS to override the estimated number of tasklets(optional)")
ap.add_argument("-d", "--nr_of_dpus", type=int,
                help="NR_DPUs to allocate (default=1)")
args = vars(ap.parse_args())


match_cost = args["match_cost"]
mismatch_cost = args["mismatch_cost"]
gap_opening = args["gap_opening"]
gap_extending = args["gap_extending"]


if match_cost > 0 or mismatch_cost <= 0 or gap_opening <= 0 or gap_extending <= 0:
    print("Wrong affine gap penalties must be  m <= 0 and g, a, x > 0\n")
    exit(-1)

read_length = args["read_length"]
if read_length <= 0:
    print("Undefined input read length")
    exit(-1)

number_reads = args["number_reads"]
if number_reads <= 0:
    print("Undefined number of input reads")
    exit(-1)

nr_of_wrong_bases = 10
if args["max_edit"] is not None:
    max_score = args["max_edit"]
    nr_of_wrong_bases = args["max_edit"]
elif args["error"] is not None:
    nr_of_wrong_bases = read_length * args["error"]
    max_score = math.ceil(max(nr_of_wrong_bases*mismatch_cost,
                          nr_of_wrong_bases*(gap_opening + gap_extending)))
else:
    print("Missing input provide either max_edit or \%\ error")
    exit(-1)

if nr_of_wrong_bases == 0:
    nr_of_wrong_bases = 1
    max_score = 1

read_length = math.ceil((((read_length + nr_of_wrong_bases) + 7)/8))*8
# memory upper limit is estimated using the bitvectors
memory_upper_limit = 80 + ((read_length + 64)/64) * \
    8*5 + 4*read_length + 68 + 1024
memory_upper_limit = int(math.ceil((((memory_upper_limit) + 7)/8))*8)


# Estimated stack memory size is 1024
# Check if the memory upper limit exceeds WRAM
for NR_TASKLETS in range(1, 19):
    if NR_TASKLETS * memory_upper_limit >= (62000 - NR_TASKLETS*1024):
        NR_TASKLETS = NR_TASKLETS-1
        break

if NR_TASKLETS == 0:
    NR_TASKLETS = 1

memory_upper_limit_mram = NR_TASKLETS*2*(nr_of_wrong_bases + 1) * ((read_length+64)/64)*8 + NR_TASKLETS*read_length*(
    nr_of_wrong_bases+1)*4*((read_length+64)/64)*8 + (number_reads/args["nr_of_dpus"])*4*read_length + (number_reads/args["nr_of_dpus"])*24
memory_upper_limit_mram = int(math.ceil((((memory_upper_limit_mram) + 7)/8))*8)

# Check if it exceeds the MRAM capacity
if memory_upper_limit_mram >= 64000000:
    for NR_TASKLETS in range(1, NR_TASKLETS):
        memory_upper_limit_mram = NR_TASKLETS*2*(nr_of_wrong_bases + 1) * ((read_length+64)/64)*8 + NR_TASKLETS*read_length*(
            nr_of_wrong_bases+1)*4*((read_length+64)/64)*8 + (number_reads/args["nr_of_dpus"])*4*read_length + (number_reads/args["nr_of_dpus"])*24
        if memory_upper_limit_mram >= 64000000:
            NR_TASKLETS = NR_TASKLETS-1
            break

if NR_TASKLETS == 0:
    NR_TASKLETS = 1

print("Estimated NR of tasklets: ", str(NR_TASKLETS))
print("Estimated nr of bytes per tasklets (WRAM): ", str(memory_upper_limit))

# If the number of tasklets is overrided in the command line
if args["nr_of_tasklets"] is not None:
    if args["nr_of_tasklets"] <= NR_TASKLETS and args["nr_of_tasklets"] >= 1:
        NR_TASKLETS = args["nr_of_tasklets"]
        memory_upper_limit = (62000 - NR_TASKLETS*1024) / NR_TASKLETS
        memory_upper_limit = int(math.ceil((((memory_upper_limit) + 7)/8))*8)

if memory_upper_limit_mram >= 64000000:
    for NR_TASKLETS in range(1, NR_TASKLETS):
        memory_upper_limit_mram = NR_TASKLETS*2*(nr_of_wrong_bases + 1) * ((read_length+64)/64)*8 + NR_TASKLETS*read_length*(
            nr_of_wrong_bases+1)*4*((read_length+64)/64)*8 + (number_reads/args["nr_of_dpus"])*4*read_length + (number_reads/args["nr_of_dpus"])*24
        if memory_upper_limit_mram >= 64000000:
            NR_TASKLETS = NR_TASKLETS-1
            break

if NR_TASKLETS == 0:
    if memory_upper_limit >= (62000 - 1024) or memory_upper_limit_mram >= 64000000:
        print("Data doesn't fit in the WRAM")
        exit(-1)
    if memory_upper_limit_mram >= 64000000:
        print("Data doesn't fit in the MRAM")
        exit(-1)
    NR_TASKLETS = 1

if memory_upper_limit >= 62000:
    memory_upper_limit = 62000

print("Number of allocated tasklets: ", str(NR_TASKLETS))
print("Number of allocated bytes per tasklets (WRAM): ", str(memory_upper_limit))

options = ""
NR_DPUs = 1
if args["nr_of_dpus"]:
    NR_DPUs = args["nr_of_dpus"]


os.system("make clean")
cmd = "make NR_DPUS="+str(NR_DPUs)+" NR_TASKLETS="+str(NR_TASKLETS)+" FLAGS=\"-DMAX_SCORE="+str(int(max_score))+" -DREAD_SIZE="+str(int(read_length))+" -DWRAM_SEGMENT=" + \
    str(memory_upper_limit)+" -DMATCH="+str(match_cost)+" -DMISMATCH="+str(mismatch_cost) + \
    " -DGAP_O="+str(gap_opening)+" -DGAP_E="+str(gap_extending) + options+"\""

os.system("echo "+str(cmd))
os.system(cmd)


cmd = "./build/host " + args["input"] + " " + \
    args["output"] + " " + str(number_reads)
os.system(cmd)
