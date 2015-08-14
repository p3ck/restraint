Jobs
====

Restraint parses a sub-set of the beaker job xml [#]_. Here is an example showing
just the elements required for running in the stand alone configuration.

::

 <job>
  <recipeSet>
   <recipe>
    <task name="/kernel/performance/fs_mark">
     <fetch url="git://fedorapeople.org/home/fedora/bpeck/public_git/tests.git?master#kernel/performance/fs_mark" />
     <params>
      <param name="foo" value="bar"/>
     </params>
    </task>
       .
       .
       .
    <task name="/kernel/foo/checker">
     <rpm name="rh-tests-kernel-foo-checker"/>
    </task>
   </recipe>
  </recipeSet>
 </job>

Naming Tasks
------------

For reporting purposes it is a good idea to name your tasks.  For git tasks I have settled on a
stadard where I use the sub-directory path from my git repo as the task name. You
can see that in the following example.

::

 <task name="/kernel/performance/fs_mark">

This name will be used when reporting on the status of the task and when reporting
results.

Installing Tasks
----------------

The above example shows that you can install tasks directly from git or from an rpm
in a yum repo.

Fetch
~~~~~

The first example shows fetching a task from git.

::

 <fetch url="git://fedorapeople.org/home/fedora/bpeck/public_git/tests.git?master#kernel/performance/fs_mark" />

 OR

 <fetch url="http://fedorapeople.org/home/fedora/bpeck/public_git/snapshot/tests.tar.gz?master#kernel/performance/fs_mark" />

The fetch node accepts git uri's that conform to the following:

* Prefixed with git:// OR use tarballs with http:// and cgit can serve them automatically.
* the fully qualified hostname, remember that the system running restraintd must be able to reach this host.
* the path to the git repo.
* Optionally you can specify a valid reference which can be a branch, tag or SHA-1. ie: ?master
* Optionally you can specify a sub-dir.  restraint will only extract this sub-dir and run the task from here. ie: #kernel/performance/fs_mark.  Notice that there is not a preceding slash here.

Restraint uses git's archive protocol to retrieve the contents so make sure your git server has enabled
this.  You can enable this on most servers by putting the following in your git repo config

::

 [daemon]
        uploadarch = true

RPM
~~~

The second example will attempt to install the task via yum/rpm.

::

 <rpm name="rh-tests-kernel-foo-checker"/>

Currently restraint does not attempt to set up any repos that you may have specified in
your job.xml.  This means that in order for it to install the above task you must have
already configured the task repo on the machine running restraintd.

Parameters
----------

You can optionally pass parameters to a task via environment variables.  The following snippet
from our example would create an environment variable name foo with the value bar.

::

 <params>
  <param name="foo" value="bar"/>
 </params>

The parameter RSTRNT_MAX_TIME allows you to specify a different max time than what
is specified in the tasks metadata.  Setting KILLTIMEOVERRIDE also has the same
affect and is provided for compatibility with legacy RHTS.

.. [#] `Beaker Job XML <http://beaker-project.org/docs/user-guide/job-xml.html>`_.
