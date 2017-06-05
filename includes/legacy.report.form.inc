<?php

function upitt_workflow_legacy_objects() {
  global $user;
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  $base_path = conf_path(TRUE);
  if ($user->uid == 1 || user_access(ISLANDORA_WORKFLOW_MANAGE_LOOKUPTABLES, $user)) {
    $options = array(); // 'attributes' => array('title' => 'Requires "Workflow Manage Lookup Tables" permission'));
    $asterisk = "<sup style='color:blue' title='Requires \"Workflow Manage Lookup Tables\" permission'>*</super>";
    $links = array(l('Make batch records from Workflow items', '/workflow/admin/makebatch_from_workflow', $options),
                   l('Workflow Admin', '/workflow/admin', $options),
                   l('Create a New Batch', '/workflow/batch/new', $options),
                   l('Manage Lookup Tables', '/workflow/admin/manage/action', $options),
                   l('Workflow', '/workflow', $options),
        );
  }

  $page = (isset($_GET['page']) ? (is_numeric($_GET['page']) ? $_GET['page'] : 0) : 0);
  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  // if there is any selection of the filters, then the data must be filtered with a WHERE clause, else don't add that condition
  $where_clause = '';

  if (isset($_GET['batch_external_id_filter']) && $_GET['batch_external_id_filter'] <> '') {
    $where_clause .= "i.name like '%" . upitt_workflow_safe_qstring($link, $_GET['batch_external_id_filter']) . "%' ";
  }

  if (isset($_GET['type_filter']) && $_GET['type_filter'] <> '') {
    $where_clause .= ($where_clause ? ' AND ' : '') . "it.name = '" . upitt_workflow_safe_qstring($link, $_GET['type_filter']) . "' ";
  }
  if (isset($_GET['collection_filter']) && $_GET['collection_filter'] <> '') {
    $where_clause .= ($where_clause ? ' AND ' : '') . "cc.name = '" . upitt_workflow_safe_qstring($link, $_GET['collection_filter']) . "' ";
  }

  $where_clause = ($where_clause) ? "WHERE " . $where_clause : ' ';

  $query = 'SELECT count(do_id) `total` ' .
           'FROM `item` i ' .
           'JOIN item_type it ON (it.id = i.type_id) ' .
           'JOIN collection cc ON (cc.id = i.primary_collection_id) ' .
           $where_clause;

  $result = mysqli_query($link, $query, MYSQLI_USE_RESULT);
  if (!$result) {
     echo(mysqli_error($link));
  }

  $row = mysqli_fetch_assoc($result);
  $count_rows = $row['total'];
  mysqli_free_result($result);

  $current_page = pager_default_initialize($count_rows, WF_OBJECTS_PER_PAGE);

  $query = 'SELECT "" as `ingested?`, i.do_id, i.name, it.name `type`, ' . //XX po.username, cc.name `collection`, ' .
           '  (SELECT GROUP_CONCAT(ta.description) FROM transaction ct JOIN transaction_actions ta ON (ta.id = ct.transaction_action_id) JOIN item_current_status cics ON (cics.last_transaction_id = ct.id) WHERE cics.item_id = i.id ORDER BY ct.timestamp DESC) `last status`, ' .
           '  (SELECT ct.timestamp FROM `transaction` ct JOIN item_current_status cics ON (cics.last_transaction_id = ct.id) WHERE cics.item_id = i.id ORDER BY ct.timestamp DESC LIMIT 1) `status date` ' .
           'FROM `item` i ' .
           'JOIN item_type it ON (it.id = i.type_id) ' .
           'JOIN collection cc ON (cc.id = i.primary_collection_id) ' .
           $where_clause .
           'LIMIT ' . (($page) ? $page * WF_OBJECTS_PER_PAGE . ', ' : '') . WF_OBJECTS_PER_PAGE;

  $result = mysqli_query($link, $query, MYSQLI_USE_RESULT);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $query);
  }

  $ob1_page = $page + 1;
  $count_rows_markup = number_format($count_rows) . " workflow objects " . (($ob1_page <> 0) ? " (page " . $ob1_page . " of " . round($count_rows / WF_OBJECTS_PER_PAGE + 0.9999) . ")" : "");
  $headings = $rows = array();

  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  while ($row = mysqli_fetch_assoc($result)) {
    if (count($headings) < 1) {
      $headings = array_keys($row);
    }
    if (array_key_exists('do_id', $row)) {
      $pid = $ingest_namespace . $row['do_id'];
      if ($obj = islandora_object_load($pid)) {
        $row['ingested?'] = l($row['do_id'], '/islandora/object/' . $pid, array('attributes' => array('target' => '_blank')));
      } else {
        $row['ingested?'] = '';
      }
      $row['do_id'] = l($row['do_id'], '/workflow/object/' . $row['do_id']);
    }
    $rows[] = $row;
  }

  mysqli_free_result($result);
  mysqli_close($link);
  $pager = theme('pager', array('quantity',$count_rows));
  $table_markup = theme('upitt_workflow_report',
      array('table' => $pager . theme('table', array('rows'=>$rows,'header'=>$headings,'attributes' =>array('class'=>array('report_table')))) . $pager,
            'count_rows' => $count_rows_markup,
            'links' => $links,
            'breadcrumb' => '',
            'post_to' => '/workflow',
            'collection_filter_choices' => upitt_workflow_get_collection_choices(),
           )
    );
  $form['table_markup'] = array('#markup' => $table_markup);
  return $form;
}

function upitt_workflow_legacy_objectreport_form($do_id) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $status = upitt_workflow_get_status($do_id);

  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  $detail_query = 'SELECT b.batch_external_id `name`, b.batch_description `title`, b.mapto_collections `collection/s`, b.batch_id `batch_id` ' .
                  'FROM `item` i ' .
                  'JOIN batch_item bi ON (bi.item_id = i.id) ' .
                  'JOIN batch b ON (bi.batch_id = b.batch_id) ' .
                  'WHERE i.do_id = \'' . upitt_workflow_safe_qstring($link, $do_id) . '\'';

  $result = mysqli_query($link, $detail_query);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $detail_query);
  }
  $collections = upitt_workflow_lookup_collection_names();

  $ingest_namespace = upitt_workflow_get_ingest_namespace(); 

  $pid = $ingest_namespace . $do_id;
  $islandora_object = islandora_object_load($pid);
  
  $object_title = (is_object($islandora_object) ? $islandora_object->label : $do_id);
  $title = "Workflow object | " . (is_object($islandora_object) ? 'Islandora: "' . l($islandora_object->label, '/islandora/object/' . $pid) . '"' : $do_id);

  $files_headings = $status_headings = $files_rows = $required_workflow_actions = $status_rows = array();
  while ($row = mysqli_fetch_assoc($result)) {
    if (isset($row['batch_id']) && isset($row['name'])) {
      $row['batch'] = l($row['name'], '/workflow/batch/' . $row['batch_id']) . '<br />';
      unset($row['batch_id']);
    }
    else {
      $row['batch'] = 'n/a';
    }
    if (isset($row['collection/s'])) {
      // this has to lookup the collection names from the array that is loaded before this loop.
      $row['collection/s'] = upitt_workflow_collection_id_map_names($row['collection/s'], $collections);
    }

    // TODO: this edit link may need to be suppressed if the batch is at a specific stage.
    $edit_item_link = l('Edit "' . $object_title . '"', '/workflow/object/' . $do_id . '/edit');

    $details = theme('workflow_item_details', array('details' => $row, 'edit_item_link' => $edit_item_link));
  }
  if (!$details) {
    drupal_set_message('Workflow object "' . $do_id . '" not found.  <a href="javascript:history.go(-1);">Return to previous page.</a>');
    drupal_goto(variable_get('site_404', ''));
  }
  $links = array();

  // if there is any selection of the filters, then the data must be filtered with a WHERE clause, else don't add that condition
  $filters_use = $filters_mimetype = '';

  if (isset($_GET['use_filter']) && $_GET['use_filter'] <> '') {
    $filters_use = upitt_workflow_filters_clause($_GET['use_filter']);
  }
  if (isset($_GET['mimetype_filter']) && $_GET['mimetype_filter'] <> '') {
    $filters_mimetype = upitt_workflow_filters_clause($_GET['mimetype_filter']);
  }

  // allow to filter on cif.mime_type and cif.use
  // Files table --
  $files_query = 'SELECT f.name `name`, f.path `path`, f.mime_type `mime_type`, f.use `use`, f.timestamp ' .
                 'FROM item_file f ' .
                 'LEFT JOIN item i ON (f.item_id=i.id) ' .
                 'WHERE i.do_id=\'' . upitt_workflow_safe_qstring($link, $do_id) . '\'' .
                 (($filters_use) ? ' AND i.use IN ' . $filters_use : '') .
                 (($filters_mimetype) ? ' AND i.mime_type IN ' . $filters_mimetype : '') .
                 ' ' .
                 'ORDER BY f.name';

  $result = mysqli_query($link, $files_query);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $files_query);
  }
  while ($row = mysqli_fetch_assoc($result)) {
    if (count($files_headings) < 1) {
      $files_headings = array_keys($row);
    }
    if (array_key_exists('do_id', $row)) {
      $pid = $ingest_namespace . $row['do_id'];
      if ($obj = islandora_object_load($pid)) {
        $row['do_id'] = l($row['do_id'], '/islandora/object/' . $pid, array('attributes' => array('target' => '_blank')));
      } else {
        $row['do_id'] = l($row['do_id'], '/workflow/object/' . $row['do_id']);
      }
    }
    $files_rows[] = $row;
  }

  $workflow_sequence_name = '';
  // Get the required workflow sequence steps from this object's batch.  It will be used in conjunction with the status_query
  // below in order to display which items remain to be performed.
  $workflow_sequence_actions = 'SELECT a.name, a.description, wsa.`order`, ws.name `workflow_sequence_name` ' .
                               'FROM item i ' . 
                               'JOIN batch_item bi ON (bi.item_id = i.id) ' .
                               'JOIN batch b ON (b.batch_id = bi.batch_id) ' . 
                               'JOIN workflow_sequence ws ON (ws.id = b.batch_sequence_id) ' . 
                               'JOIN workflow_sequence_actions wsa ON (wsa.workflow_sequence_id = ws.id) ' .
                               'JOIN `action` a ON (a.id = wsa.action_id) ' .
                               'WHERE i.do_id = \'' . upitt_workflow_safe_qstring($link, $do_id) . '\' ' .
                               'ORDER BY wsa.`order`';
  $result = mysqli_query($link, $workflow_sequence_actions);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $workflow_sequence_actions);
  }
  while ($row = mysqli_fetch_assoc($result)) {
    $required_workflow_actions[] = $row;
    $workflow_sequence_name = $row['workflow_sequence_name'];
  }

  // simple status ~ transactions table
  $status_query = 'SELECT ta.description, t.`timestamp` `time` '.
                  'FROM item i ' .
                  'JOIN transaction t ON (t.item_id = i.id) ' .
                  'JOIN transaction_actions ta ON (ta.id = t.transaction_action_id) ' .
//                  'JOIN collection co ON (co.id = i.primary_collection_id) ' .
                  'WHERE i.do_id=\'' . upitt_workflow_safe_qstring($link,  $do_id) . '\' ' .
                  'ORDER BY i.do_id DESC, t.`timestamp` ASC';

  $result = mysqli_query($link, $status_query);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $status_query);
  }
  while ($row = mysqli_fetch_assoc($result)) {
    if (count($status_headings) < 1) {
      $status_headings = array_keys($row);
      $status_headings[] = 'when';
    }
    $row['when'] = upitt_workflow_timeago_from_timestamp($row['time']);
    $status_rows[] = $row;
  }
  mysqli_close($link);

  $status_table_problems = array();
  foreach ($required_workflow_actions as $required_workflow_action) {
    $completed_action_name = $required_workflow_action['name'] . ' completed';
    $completed_action_found = FALSE;
    foreach ($status_rows as  $k => $status_row) {
      if ($status_row['description'] == $completed_action_name) {
        $status_rows[$k]['description'] = '<span style="color:green">' . $status_rows[$k]['description'] . '</i>';
        $completed_action_found = TRUE;
      }
    }
    if (!$completed_action_found) {
      $status_table_problems[] = $required_workflow_action['name'] . ' not completed';
    }
  }
  foreach ($status_table_problems as $problem) {
    $status_rows[] = array('description' => '<span style="color:red">' . str_replace(" not completed", "", $problem) . '</span>', 'time' => '', 'when' => '');
  }

  if (module_exists('islandora_mets_editor')) {
    $links[] = l('Edit METS', 'islandora/object/' . $ingest_namespace . $do_id . '/manage/mets_editor');
  }
  $links[] = l('Islandora Ingest', '/workflow/object/' . $do_id . '/islandora_ingest');

  $table_markup = theme('upitt_workflow_report_with_detail_table',
      array('details' => $details,
            'workflow_sequence_name' => $workflow_sequence_name,
            'status_table' => theme('table', array('rows'=>$status_rows,'header'=>$status_headings)),
//  ((count($status_table_problems) > 0) ? '<div class="messages error">' . implode('</div><div class="messages error">', $status_table_problems) . '</div>' : '') . theme('table', array('rows'=>$status_rows,'header'=>$status_headings)),
            'files_table' => theme('table', array('rows'=>$files_rows,'header'=>$files_headings)),
            'title' => $title,
            'breadcrumb' => upitt_workflow_get_breadcrumb_path(),
            'links' => $links,
            'post_to' => '/workflow/object/' . $do_id,
           )
    );

  $content = array(
    'table_markup' => array('#markup' => $table_markup),
  );
  return $content;
}
