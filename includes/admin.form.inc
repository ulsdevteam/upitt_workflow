<?php

/**
 * Controller function for /workflow/admin/manage
 */
function upitt_workflow_admin() {
  $links = upitt_workflow_get_listname_manage_links('');
  $breadcrumb = upitt_workflow_get_breadcrumb_path();

  $page = array();

  $page['#markup'] = theme('upitt_workflow_admin', array('links' => $links, 'breadcrumb' => $breadcrumb));

  return $page;
}
