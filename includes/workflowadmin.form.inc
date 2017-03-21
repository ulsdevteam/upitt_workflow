<?php

/**
 * Controller function for /workflow/workflowadmin/manage
 */
function upitt_workflow_workflowadmin() {
  $links = upitt_workflow_get_listname_manage_links('');
  $breadcrumb = upitt_workflow_get_breadcrumb_path();
  $page = array();

  $task_links[] = l(t('Create Islandora Collection'), '/islandora/object/pitt%3Aroot/manage/overview/ingest');
  $task_links[] = l(t('Create New Batch'), '/workflow/batch/new');
  $task_links[] = l(t('[online] Collection Building'), '/workflow/collection_building');
  $task_links[] = l(t('Quick Status Change'), '/workflow/status/change');

  $page['#markup'] = theme('upitt_workflow_workflowadmin', array('links' => $links, 'task_links' => $task_links, 'breadcrumb' => $breadcrumb));

  return $page;
}
