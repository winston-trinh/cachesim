# Cache Simulator
### Welcome!
I developed this project to demonstrate my understanding of cache memory and its role in improving C program performance. I've written a cache simulator to create a concise and effective program that simulates cache behavior.

### Purpose
My cache simulator is a tool that models how cache works. It's particularly useful for visualizing the efficiency of caches in computing. Although this simulator can seem technical, at its core, this project is about improving how quickly and efficiently a computer can access data.

## How It Works
### Trace Files
The simulator uses trace files from the `traces` directory. These files, generated by a tool called `valgrind`, record how programs access memory. Here's an example command used to create a trace file:

```bash
valgrind --log-fd=1 --tool=lackey -v --trace-mem=yes ls -l
```

The trace files look like this:

```
I 0400d7d4,8
 M 0421c7f0,4
 L 04f6b868,8
 S 7ff0005c8,8
```

Each line represents a memory access, showing the type of access (`I` for instruction load, `L` for data load, etc.) and the memory address involved.

### Command-Line Interface
My simulator, which is in the file `csim.c`, imitates the behavior of these caches. It's run using the terminal, and it outputs statistics like the number of hits, misses, and evictions. Here's how you use it:

```bash
./csim-ref [-hv] -S <S> -K <K> -B <B> -p <P> -t <tracefile>
```

### Cache Organization
The simulator is flexible, able to work with various cache sizes and structures. It dynamically allocates memory, and I've implemented both FIFO (First-In, First-Out) and LRU (Least Recently Used) policies for cache eviction.

### Testing
I've tested my simulator against different benchmarks, all of which can be found in the `traces` folder. These tests include direct-mapped tests (K = 1), policy tests (check to see if LFU and FIFO work as expected), and size tests (cases that included memory accesses crossing a cache line and requiring access to different blocks)

To run these tests, I used the command:

```bash
$ ./grade
```

## Where to Start
I encourage you to look at my `csim.c` file to see how I've approached this problem.  

I'm happy to share my learning journey and experiences in this area or discuss the strategies I used to design my cache simulator.

Please message me if you would like to talk or share your thoughts. Thank you for taking the time to explore my project!

Contact: [wntrinh@usc.edu](mailto:wntrinh@usc.edu)
