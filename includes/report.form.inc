<?php

function upitt_workflow_objects() {
  global $user;
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  $base_path = conf_path(TRUE);
  if ($user->uid == 1 || user_access(ISLANDORA_WORKFLOW_MANAGE_LOOKUPTABLES, $user)) {
    $options = array('attributes' => array('title' => 'Requires "Workflow Manage Lookup Tables" permission'));
    $asterisk = "<sup style='color:blue' title='Requires \"Workflow Manage Lookup Tables\" permission'>*</super>";
    $links = array(l('Workflow Admin', '/workflow/admin', $options),
                   l('Create a New Batch', '/workflow/batch/new', $options),
                   l('Manage Lookup Tables', '/workflow/admin/manage/action', $options),
                   l('Workflow (legacy)', '/workflow_legacy', $options),

        );
  }

  $table_markup = theme('upitt_workflow_report',
      array('table' => '',
            'links' => $links,
            'breadcrumb' => '',
            'post_to' => '',
            'collection_filter_choices' => upitt_workflow_get_collection_choices(),
           )
    );
  $form['table_markup'] = array('#markup' => $table_markup);
  return $form;
}
