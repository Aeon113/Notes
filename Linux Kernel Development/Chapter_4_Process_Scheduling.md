# Process Scheduling

Policy is the behavior of the scheduler that determines what runs when. A scheduler's policy often determines the overall feel of a system and is responsible for optimally utilizing processor time.     

----------------------------

Processes can be classified as either `I/O-bound` or `processor-bound`.     

I/O-bound proceses spend much of its time submitting and waiting on I/O requests, therefore they are usually runnable for only short durations, because they tend to be blocked for waiting for I/O.    

processor-bound processes spend much of their time executing code. They tend to run until they are preempted because they do not block on I/O requests very often. For these processes, the scheduler shall run them less frequently but for longer durations.    

And of course, these classifications are not mutually exclusive. Some processes are bound I/O and processor intense, and some may be in either of these two characters periodly.     

The scheduler must attempt to satisfy two conflicting goals: fast process response (low latency) and maximal system utilication (high throughput). Current Linux scheduler favors I/O-bound processes over processor-bound proceses in order to provide good interactive response and desktop performance.    

