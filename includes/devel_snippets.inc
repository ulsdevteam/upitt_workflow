<?php

/*
 * These snippets can each be taken out and pasted into the /devel/php window
 */

/* ------------------------------------------------------------------------------- */
// The value for the identifier part of any PID where
//  {PID} = {namespace}:{IDENTIFIER} 
//  for example, pitt:31735070099654 would have an 
//  identifier value of "31735070099654".
$v = array('31735033464482');
module_load_include('inc', 'upitt_workflow', 'includes/utilities');
foreach ($v as $pid) {
  $solr_query = "PID:*" . $pid . "*";
  dpm('Query to find all indexed child objects related to 
  - ' . $solr_query);
  $child_pids = upitt_workflow_get_solr_options($solr_query, 'PID', 'PID');
  dpm($child_pids);
}

/* ------------------------------------------------------------------------------- */

