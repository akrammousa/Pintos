//#include "thread.h"
//#include <debug.h>
//#include <stddef.h>
//#include <random.h>
//#include <stdio.h>
//#include <string.h>
//#include "flags.h"
//#include "interrupt.h"
//#include "intr-stubs.h"
//#include "palloc.h"
//#include "switch.h"
//#include "synch.h"
//#include "vaddr.h"
//#include "thread.h"
#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "devices/timer.h"
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "thread.h"
#include "vaddr.h"

#define q 14 // this is a precision for the floating point implementation (p.q = 17.14 )

#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };




/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
int adjust_priority(int);
static tid_t allocate_tid (void);

//now islam will put the floating point functions headers
int convert_int_to_fixed(int);
int convert_fixed_to_int(int);
int add_two_fixed(int , int);
int sub_two_fixed(int , int);
int mul_two_fixed(int , int);
int mul_one_fixed_one_int(int , int);
int div_two_fixed(int, int);
int div_one_fixed_one_int(int , int );
int div_two_integer(int, int);

int add_one_fixed_one_int(int x, int n) ;

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);


  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  //habda2 men hena
    if(thread_mlfqs) {
      if(strcmp(t->name,"idle")!=0) {
          t->recent_cpu = add_one_fixed_one_int(t->recent_cpu, 1);
      }
      if (timer_ticks() % (TIMER_FREQ) == 0) {
          // load_avg = convert_fixed_to_int(
          //                  mul_one_fixed_one_int(div_two_integer(59,60), load_avg) +
          //                  mul_one_fixed_one_int(div_two_integer(1,60), (int)(list_size(&ready_list) + 1)));
          if (!strcmp(running_thread()->name, "idle") == 0) {
              load_avg = div_one_fixed_one_int(mul_one_fixed_one_int(load_avg, 59), 60) +
                         div_one_fixed_one_int(convert_int_to_fixed(list_size(&ready_list) + 1), 60);
          } else {
              load_avg = div_one_fixed_one_int(mul_one_fixed_one_int(load_avg, 59), 60) +
                         div_one_fixed_one_int(convert_int_to_fixed(list_size(&ready_list)), 60);
          }
          struct list_elem *e;
          int div_must_be_first = div_two_fixed(mul_one_fixed_one_int(load_avg,2),add_one_fixed_one_int(mul_one_fixed_one_int(load_avg,2),1));
          for (e = list_begin(&all_list); e != list_end(&all_list); e = list_next(e)) {
              struct thread *f = list_entry (e, struct thread, elem);
              f->recent_cpu = add_one_fixed_one_int(mul_two_fixed(div_must_be_first,f->recent_cpu),f->nice_value);
          }

      }

      if (timer_ticks() % 4 == 0) {
          struct list_elem *e;
          enum intr_level old_level;
          old_level = intr_disable ();
          for (e = list_begin(&all_list); e != list_end(&all_list); e = list_next(e)) {
              struct thread *f = list_entry (e, struct thread, elem);
              f->priority =adjust_priority(
                      PRI_MAX - convert_fixed_to_int(div_one_fixed_one_int(f->recent_cpu, 4)) - (f->nice_value * 2));
          }
          intr_set_level(old_level);
          intr_yield_on_return();
      }
    } else {
        /* Enforce preemption. */
        if (++thread_ticks >= TIME_SLICE)
            intr_yield_on_return();
    }

}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /* Add to run queue. */
  thread_unblock (t);
    if(t->priority > thread_current()->priority){
        thread_yield();
    }
  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
    //printf(" list size %d \n" , list_size(&ready_list));
    //printf("priority %d came \n" , t->priority);
  //replaced with list_insert for priority
    if(!list_empty(&ready_list)) {
        struct list_elem *tempNode;
        bool inserted=false;
        for (tempNode = list_begin(&ready_list); tempNode != list_end(&ready_list);
             tempNode = list_next(tempNode)) {
            struct thread *tempThread = list_entry(tempNode, struct thread, elem);
            if (tempThread->priority < t->priority){
                list_insert(&tempThread->elem , &t->elem);
                inserted =true;
                break;
            }
        }
        if (!inserted){
            list_push_back (&ready_list, &t->elem);
        }
    }
    else{
        list_push_back (&ready_list, &t->elem);
    }
//  list_push_back (&ready_list, &t->elem);
  t->status = THREAD_READY;

//   if(thread_current()->priority < t->priority){
//       thread_yield();
//   }
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread)
      if(!list_empty(&ready_list)) {
          struct list_elem *tempNode;
          bool inserted=false;
          for (tempNode = list_begin(&ready_list); tempNode != list_end(&ready_list);
               tempNode = list_next(tempNode)) {
              struct thread *tempThread = list_entry(tempNode, struct thread, elem);
              if (tempThread->priority < cur->priority){
                  list_insert(tempNode , &cur->elem);
                  inserted =true;
                  break;
              }
          }
          if (!inserted){
              list_push_back (&ready_list, &cur->elem);
          }
      }
      else{
          list_push_back (&ready_list, &cur->elem);
      }
        //list_push_back (&ready_list, &cur->elem);
//  list_push_back()
    cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
    enum intr_level old_level;
    old_level = intr_disable();
//    printf ("ready size %d \n", list_size(&ready_list));
//    struct thread *tempThread = list_entry(list_front(&ready_list), struct thread, elem);
//    printf ("current Top: %d \n", tempThread->priority);
//    printf("current before setting: %d \n",thread_current()->priority);
//
//        printf("priority %d will be changed , thread num =%d \n" , thread_current()->priority , thread_current()->tid);

    thread_current ()->priority = new_priority;
//    printf("current after setting inner: %d \n",thread_current()->priority);
    thread_yield();
    //printf("priority %d changed , thread num =%d \n" , thread_current()->priority , thread_current()->tid);
    intr_set_level (old_level);

}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) 
{
  thread_current() -> nice_value = nice;
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
    return thread_current() -> nice_value;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  return convert_fixed_to_int(mul_one_fixed_one_int(load_avg,100));
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
    return convert_fixed_to_int(mul_one_fixed_one_int(thread_current()->recent_cpu,100));
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;

  if(thread_mlfqs) { //hasta3'al 3al advanced
      if(strcmp(t->name, "main") == 0) {
          t->recent_cpu = 0;
          t->nice_value = 0;
      } else {
          t->nice_value = thread_current()->nice_value;
          t->recent_cpu = thread_current()->recent_cpu/100; //todo -- recheck in case of error ---no need to float ---
      }
      priority = adjust_priority(PRI_MAX - convert_fixed_to_int(div_one_fixed_one_int(t->recent_cpu, 4)) - (t->nice_value * 2)); // todo 2slay
  } else
  {
      t->priority = priority;
  }
  t->startPriority = priority;
  list_init(&t->myValues);
  t->doneeElem = NULL;
  t->magic = THREAD_MAGIC;

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}
void add_in_front_in_ready_queue(struct thread *donee){
    list_remove(&donee->elem);
    list_push_front(&ready_list,&donee->elem);
}

void insert_in_order_in_ready_queue(struct thread *donee){
    list_remove(&donee->elem);
    if(!list_empty(&ready_list)) {
        struct list_elem *tempNode;
        bool inserted=false;
        for (tempNode = list_begin(&ready_list); tempNode != list_end(&ready_list);
             tempNode = list_next(tempNode)) {
            struct thread *tempThread = list_entry(tempNode, struct thread, elem);
            if (tempThread->priority <= donee->priority){
                list_insert(tempNode , &donee->elem);
                inserted =true;
                break;
            }
        }
        if (!inserted){
            list_push_back (&ready_list, &donee->elem);
        }
    }
    else{
        list_push_back (&ready_list, &donee->elem);
    }
}

//islam will put the floating point functions.

int add_one_fixed_one_int(int x, int n) {
    return x + (16384 * n);
}
int convert_int_to_fixed(int x) {
    return x*16384;
}
int convert_fixed_to_int(int x) {
    if(x >= 0) {
        return (x + 8192) / 16384;
    } else {
        return (x - 8192) / 16384;
    }
}
int add_two_fixed(int x, int y) {
    return x + y;
}
int sub_two_fixed(int bigger, int smaller) {
    return bigger - smaller;
}
int mul_two_fixed(int x, int y) {
    return ((int64_t) x ) * y / 16384;
}
int mul_one_fixed_one_int(int x, int n) {
    return x*n;
}
int div_two_fixed(int x, int y) {
    return ((int64_t) x ) * 16384 / y;
}
int div_one_fixed_one_int(int fixed, int integer) {
    return fixed/integer;
}
int div_two_integer(int x, int y) {
    x = convert_int_to_fixed(x);
    y = convert_int_to_fixed(y);
    return div_two_fixed(x,y);
}
int adjust_priority(int p) {
    if(p >=0 && p <= 63) {
        return p;
    } else if (p < 0) {
        return 0;
    } else {
        return 63;
    }
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
