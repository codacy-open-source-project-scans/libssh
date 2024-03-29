#include "config.h"

#define LIBSSH_STATIC

#include "torture.h"
#include "sftp.c"

#include <sys/types.h>
#include <pwd.h>
#include <errno.h>

static int sshd_setup(void **state)
{
    torture_setup_sshd_server(state, false);

    return 0;
}

static int sshd_teardown(void **state)
{
    torture_teardown_sshd_server(state);

    return 0;
}

static int session_setup(void **state)
{
    struct torture_state *s = *state;
    struct passwd *pwd = NULL;
    int rc;

    pwd = getpwnam("bob");
    assert_non_null(pwd);

    rc = setuid(pwd->pw_uid);
    assert_return_code(rc, errno);

    s->ssh.session = torture_ssh_session(s,
                                         TORTURE_SSH_SERVER,
                                         NULL,
                                         TORTURE_SSH_USER_ALICE,
                                         NULL);
    assert_non_null(s->ssh.session);

    s->ssh.tsftp = torture_sftp_session(s->ssh.session);
    assert_non_null(s->ssh.tsftp);

    return 0;
}

static int session_teardown(void **state)
{
    struct torture_state *s = *state;

    torture_rmdirs(s->ssh.tsftp->testdir);
    torture_sftp_close(s->ssh.tsftp);
    ssh_disconnect(s->ssh.session);
    ssh_free(s->ssh.session);

    return 0;
}

static void torture_sftp_expand_path(void **state)
{
    struct torture_state *s = *state;
    struct torture_sftp *t = s->ssh.tsftp;
    struct passwd *pwd = NULL;
    char *expanded_path = NULL;
    int rc;

    rc = sftp_extension_supported(t->sftp, "expand-path@openssh.com", "1");
    if (rc == 0) {
        skip();
    }

    pwd = getpwnam(TORTURE_SSH_USER_ALICE);
    assert_non_null(pwd);

    /* testing for a absolute path */
    expanded_path = sftp_expand_path(t->sftp, "~/.");
    assert_non_null(expanded_path);

    assert_string_equal(expanded_path, pwd->pw_dir);

    SSH_STRING_FREE_CHAR(expanded_path);

    /* testing for a relative path */
    expanded_path = sftp_expand_path(t->sftp, ".");
    assert_non_null(expanded_path);

    assert_string_equal(expanded_path, pwd->pw_dir);

    SSH_STRING_FREE_CHAR(expanded_path);

    /* passing a NULL sftp session */
    expanded_path = sftp_expand_path(NULL, "~/.");
    assert_null(expanded_path);

    /* passing an invalid path */
    expanded_path = sftp_expand_path(t->sftp, "/...//");
    assert_null(expanded_path);

    /* passing null path */
    expanded_path = sftp_expand_path(t->sftp, NULL);
    assert_null(expanded_path);
}

int torture_run_tests(void)
{
    int rc;
    struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(torture_sftp_expand_path,
                                        session_setup,
                                        session_teardown)
    };

    ssh_init();

    torture_filter_tests(tests);
    rc = cmocka_run_group_tests(tests, sshd_setup, sshd_teardown);

    ssh_finalize();

    return rc;
}
