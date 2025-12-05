# Timing Analysis: LCD Mutex and Thread Execution

## ECE 460/560 Final Project - Timing Measurements

---

## 1. Logic Analyzer Screenshots

Two screenshots from WaveForms logic analyzer showing each thread accessing the LCD mutex:

### Screenshot 1: Thread_Draw_Waveforms Accessing LCD Mutex
- **Signal Configuration:**
  - LCD Blocking (DIO 8) - Red: Shows pulse when thread is acquiring/blocking on LCD mutex
  - T_Draw_Waveforms (DIO 6) - Purple: Thread execution signal (rise to fall)
  - T_Draw_UI_Controls (DIO 7) - Blue: Thread execution signal (rise to fall)

- **Observation:** Thread_Draw_Waveforms shows a pulse on the LCD Blocking signal, indicating it successfully acquired the mutex. The blocking time varies from 12.5 µs (immediate acquisition) to 29.025 ms (waiting for other thread).

### Screenshot 2: Thread_Draw_UI_Controls Accessing LCD Mutex
- **Signal Configuration:** Same as above
- **Observation:** Thread_Draw_UI_Controls shows consistent 12.5 µs mutex acquisition time, indicating it typically acquires the mutex without contention.

---

## 2. Timing Statistics

### Measurement Table

|                                                              | Thread_Draw_Waveforms |         |          | Thread_Draw_UI_Controls |         |          |
|--------------------------------------------------------------|----------------------:|--------:|---------:|------------------------:|--------:|---------:|
|                                                              | Minimum               | Average | Maximum  | Minimum                 | Average | Maximum  |
| **Time accessing or blocking on LCD mutex**                  | 12.500 µs             | 2.632 ms | 29.025 ms | 12.500 µs              | 12.500 µs | 12.500 µs |
| **Thread execution time (from rise to fall of debug signal)**| 20.862 ms             | 23.238 ms | 50.413 ms | 34.737 ms              | 38.880 ms | 58.212 ms |

---

## 3. Analysis: How Timing Statistics Are Related

### Relationship Between Statistics

1. **Minimum Blocking Time (12.500 µs) = Mutex Acquisition Overhead**
   - This is the time required for each possible mutex access when the `LCD_mutex` is immediately available
   - Represents the RTOS overhead for `osMutexAcquire()` and `osMutexRelease()` calls
   - Both threads show this same minimum, confirming it's the inherent mutex operation cost

2. **Thread Execution Time Includes Blocking Time**
   - The thread execution time (rise to fall of debug signal) includes:
     - Time waiting to acquire the mutex (blocking)
     - Time spent performing LCD operations while holding the mutex
     - Any other thread processing
   - Maximum execution times (50.4 ms and 58.2 ms) are higher than averages due to mutex contention

3. **Blocking Time Correlation**
   - Thread_Draw_Waveforms maximum blocking time (~29 ms) ≈ Thread_Draw_UI_Controls average execution time (~39 ms)
   - This makes sense: when Thread_Draw_Waveforms blocks, it waits for Thread_Draw_UI_Controls to finish its LCD operations

### Unexpected Statistics

**Yes, some statistics are unexpected:**

1. **Thread_Draw_UI_Controls shows NO variation in blocking time (always 12.5 µs)**
   - This is unexpected because both threads compete for the same mutex
   - **Possible explanation:** Thread_Draw_UI_Controls may have higher priority or its timing aligns such that it rarely encounters Thread_Draw_Waveforms holding the mutex

2. **Thread_Draw_Waveforms has high blocking variance (12.5 µs to 29 ms)**
   - The 2000x variation indicates significant mutex contention
   - This thread frequently has to wait for Thread_Draw_UI_Controls

3. **Thread_Draw_UI_Controls has longer execution time but shorter blocking**
   - Thread_Draw_UI_Controls: ~39 ms average execution, 12.5 µs blocking
   - Thread_Draw_Waveforms: ~23 ms average execution, 2.6 ms average blocking
   - **Implication:** Thread_Draw_UI_Controls does more LCD work per iteration, but acquires the mutex more easily

### Key Insight: Priority Inversion Risk

The data suggests that Thread_Draw_Waveforms experiences significant delays waiting for Thread_Draw_UI_Controls. If Thread_Draw_Waveforms has time-critical responsibilities, this blocking could impact system performance. The minimum blocking time (12.5 µs) represents the best-case overhead when implementing mutex-protected shared resources.

---

## 4. Summary

| Metric | Thread_Draw_Waveforms | Thread_Draw_UI_Controls |
|--------|----------------------|------------------------|
| Mutex contention experienced | High (up to 29 ms wait) | None observed |
| Average execution time | 23.2 ms | 38.9 ms |
| Mutex overhead (best case) | 12.5 µs | 12.5 µs |

The timing analysis reveals that proper mutex usage protects the LCD resource but introduces variable blocking delays. The 12.5 µs minimum blocking time is the unavoidable overhead for thread-safe LCD access.
