extern dlclose;
extern dlopen;
extern dlsym;
extern execve;
extern exit [[noreturn]];
extern fork;
extern fprintf;
extern printf;
extern __stderrp;
extern waitpid;
extern optind;
extern optreset;

X_MAIN = 0xABCD;

if !(pid = fork()) goto child;
    if !(pid + 1) goto error;
	waitpid(pid, &status, 0);
	printf("got status %x for pid %d\n", status, pid);
	if !(h = dlopen("other_program", 5)) goto done;
	    mh = dlsym(h, "_mh_execute_header");
	    my_main = mh + X_MAIN;
	    printf("my_main = %x\n", my_main);
	    *optreset = *optind = 1;
	    rv = my_main(1, { "other_program", 0 });
	    fprintf(*__stderrp, "rv = %x", rv);
	    dlclose(h);
	done:
	exit(0);
    error:
    fprintf(*__stderrp, "fork failed\n");
    exit(-1);
child:
const args = { "program", "firstarg", 0 };
exit(execve(*args, args, 0));
