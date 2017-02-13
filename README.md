# upitt_workflow
There is no install (database schema) for this module because the backend data comes from the legacy DRL site.  The data should be synced by running the migrate script again: /usr/local/src/islandora_tools/workflow_django_migrate/migrate.php

Using the xonomy javascript library to provide an XML editor.  This library is stored in /libraries/xonomy.

Also, the xonomy library does not work with jquery 1.4.4, so the jquery 3.1.1 has been added under /misc/jquery-3.1.1.min.js and is loaded explicitely for this module ONLY for the workflow/object/%/METS/edit page.
