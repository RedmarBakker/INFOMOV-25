Assignment 1 by <br>
Bjarne Daems (SNR: 6138748) <br>
Redmar Bakker (SNR:)

*Build instructions:*
- First bootstrap (as per the assignment)
- Change the current directory to ``evolution25`` <br>
- Run `` sh build.sh `` in the terminal

*Metrics: <br>*
The tables below denote the peak lps during the start of a run 
and after running for 60 seconds for a total of 3 runs.

peak lps **unoptimized** (start - run for 60s)
- [79.699 - 86.864]
- [76.698 - 83.058]
- [79.136 - 85.058]<br>
Averages:
- [78.511 - 84.993]

peak lps **optimized** (start - run for 60s)
- [176.914 - 188.034]
- [174.403 - 193.044]
- [172.814 - 194.196]<br>
Averages:
- [174.710 - 191.758]

*Speedup:*<br>
Start = 174.710/78.511 = **2,23** <br>
After 60s = 191.758/84.993 = **2,26** <br>
The expected speedup is therefore **2,25** (rounded up) 

*Notes on the assignment:* <br>
SIMD <br>
We tried to vectorize the instructions on the RGB values in the DrawWuLine function.
Although our implementation did not result in a speedup (and is not used in this version),
we still left the functions in our code for feedback which could help in future assignments.

Getting Started <br>
Since we both worked on MacBooks (MacOS) with M1/2 processors (ARM) we encountered a lot of difficulty starting with the actual assignment. 
We could only started working on it in the last week before the deadline. And could only work on it from one laptop (since it still doesn't work on Redmar's).
This severely limited our ability to work on this assignment.





