University of Pittsburgh Workflow (upitt_workflow)
=============

### Dependencies
This modules requires several modules in order to run, but it also requires that some of those modules have been patched.  The most noteworthy of these are the islandora_batch and islandora_book_batch modules.  Both of these need to handle additional parameters in order to ingest our objects as we need them.

1. islandora_batch:  https://github.com/ulsdevteam/islandora_batch/tree/uls_customizations 
2. islandora_book_batch:  https://github.com/ulsdevteam/islandora_book_batch/tree/uls_install 
3. 


#### Additional Parameters for Patched Modules
`isMemberOfSite` - must be set on our objects in order to have them appear on given sites.
`overwrite_existing` - when passed, this will overwrite any objects that may exist in the system already at the point in time of the ingest.
`assignPID` - this will allow an object to be given a set PID value when it is ingested.



There is no install (database schema) for this module because the backend data comes from the legacy DRL site.  The data should be synced by running the migrate script again: /usr/local/src/islandora_tools/workflow_django_migrate/migrate.php


## Author / License

Written by [Willow Gillingham](https://github.com/bgilling) for the [University of Pittsburgh](http://www.pitt.edu).  Copyright (c) University of Pittsburgh.

Released under a license of GPL v2 or later.
