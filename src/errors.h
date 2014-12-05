#define RESTRAINT_ERROR restraint_error_quark()
GQuark restraint_error_quark (void);

typedef enum {
    RESTRAINT_PARSE_ERROR_BAD_SYNTAX, /* parse errors */
    RESTRAINT_MISSING_FILE, /* Missing file*/
    RESTRAINT_OPEN, /* Unable to open file*/
    RESTRAINT_ALREADY_RUNNING_RECIPE_ERROR,
    RESTRAINT_TASK_RUNNER_WATCHDOG_ERROR,
    RESTRAINT_TASK_RUNNER_STDERR_ERROR,
    RESTRAINT_TASK_RUNNER_FORK_ERROR,
    RESTRAINT_TASK_RUNNER_CHDIR_ERROR,
    RESTRAINT_TASK_RUNNER_EXEC_ERROR,
    RESTRAINT_TASK_RUNNER_RC_ERROR,
    RESTRAINT_TASK_RUNNER_RESULT_ERROR,
    RESTRAINT_TASK_RUNNER_FETCH_ERROR,
    RESTRAINT_TASK_RUNNER_SCHEMA_ERROR,
    RESTRAINT_TASK_RUNNER_ABORTED,
} RestraintError;

