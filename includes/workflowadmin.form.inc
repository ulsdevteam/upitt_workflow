<?php

/**
 * Controller function for /workflow/workflowadmin/manage
 */
function upitt_workflow_workflowadmin() {
  $links = upitt_workflow_get_listname_manage_links('');
  $breadcrumb = upitt_workflow_get_breadcrumb_path();
  $page = array();

  $task_links = upitt_workflow_get_basic_links();

  $page['#markup'] = theme('upitt_workflow_workflowadmin', array('links' => $links, 'task_links' => $task_links, 'breadcrumb' => $breadcrumb));

  return $page;
}
