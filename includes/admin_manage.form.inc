<?php

function upitt_workflow_get_tablefield_stats($listname, $link_new) {
  $records = 'SELECT * ' .
             'FROM `' . mysqli_real_escape_string($link_new, $listname) . '` LIMIT 1';

  $result = mysqli_query($link_new, $records);
  $primary_key_field = '';
  $fields = $integer_fieldnames = $non_integer_fieldnames = array();

  $row = mysqli_fetch_assoc($result);
  $fields = array_keys($row);
  if (count($fields) < 1) {
    return array(NULL, array(), array());
  }    

  foreach ($fields as $field) {
    $sql_fieldtype = "SHOW FIELDS FROM `" . mysqli_real_escape_string($link_new, $listname) . "` where Field = '" . $field . "'";
    $result = mysqli_query($link_new, $sql_fieldtype);
    while ($row = mysqli_fetch_assoc($result)) {
      if (!$primary_key_field && $row['Key'] == 'PRI') {
        $primary_key_field = $field;
      }
      if (substr($row['Type'], 0, 4) == "int(" || substr($row['Type'], 0, 7) == 'TINYINT') {
        $integer_fieldnames[] = $field;
      } else {
        $non_integer_fieldnames[] = $field;
      }
    }
  }
  return array($primary_key_field, $integer_fieldnames, $non_integer_fieldnames, $fields);
}

function upitt_workflow_get_table_record($listname, $primary_key_field, $primary_id, $link_new) {
  $sql_record = "SELECT  * FROM `" . mysqli_real_escape_string($link_new, $listname) . "` where `" . $primary_key_field . "` = '" . $primary_id . "'";
dpm($sql_record);
  $result = mysqli_query($link_new, $sql_record);
  return ($row = mysqli_fetch_assoc($result)) ? $row : array();
}

/**
 * Controller function for /workflow/admin/manage/%listname%
 *
 *  The access callback should be allowing only specific listnames through including: 'collection', 'content_types', 'item_type', 'property_owner', 'workflow_sequence', 'workflow_sequence_actions'
 *
 *   new                               lookup table?
 *  -----------------------------------------------------
 *   action
 *   batch
 *   batch_item
 *   collection                        YES
 *   content_types                     YES
 *   item
 *   item_current_status
 *   item_file
 *   item_type                         YES
 *   property_owner
 *   transaction
 *   workflow_sequence                 YES
 *   workflow_sequence_actions         YES (moreless)
 *  
 */
function upitt_workflow_admin_manage($listname = '') {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $records_headings = $rows = $links = $values = array();

  $links = upitt_workflow_get_listname_manage_links($listname);
  $breadcrumb = upitt_workflow_get_breadcrumb_path();

  $page = (isset($_GET['page']) ? (is_numeric($_GET['page']) ? $_GET['page'] : 0) : 0);
  $link_new = upitt_workflow_get_databaselink('mysql_new_workflow');

  $records = 'SELECT * ' .
             'FROM `' . mysqli_real_escape_string($link_new, $listname) . '` ' .
             'LIMIT ' . (($page) ? $page * WF_OBJECTS_PER_PAGE . ', ' : '') . WF_OBJECTS_PER_PAGE;

  if ($listname == 'workflow_sequence_actions') {
    $records = 'SELECT wsa.id `id`, wsa.workflow_sequence_id `workflow_sequence_id`, wsa.action_id `action_id`, wsa.`order` `order`, ws.name, ws.description ' .
               'FROM `workflow_sequence_actions` wsa ' .
               'JOIN workflow_sequence ws ON (ws.id = wsa.workflow_sequence_id) ' .
               'ORDER BY wsa.workflow_sequence_id, wsa.`order` ' . 
               'LIMIT ' . (($page) ? $page * WF_OBJECTS_PER_PAGE . ', ' : '') . WF_OBJECTS_PER_PAGE;
      // str_replace(array(' FROM ', ' LIMIT '), array(', ws.name, ws.description FROM ', ' JOIN workflow_sequence ws ON (ws.id = workflow_sequence_id) ORDER BY workflow_sequence_id, `order` LIMIT '), $records);
  }

  $query = 'SELECT count(*) as `total` ' .
           'FROM `' . mysqli_real_escape_string($link_new, $listname) . '`';

  $result = mysqli_query($link_new, $query);

  $row = mysqli_fetch_assoc($result);
  $count_rows = $row['total'];
  $current_page = pager_default_initialize($count_rows, WF_OBJECTS_PER_PAGE);
  $result = mysqli_query($link_new, $records);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $records);
  }

  while ($row = mysqli_fetch_assoc($result)) {
/*    if (count($fields) < 1) {
      $fields = array_keys($row);
    }
*/
    if (isset($row['name']) && isset($row['batch_id'])) {
      $row['name'] = l($row['name'], '/workflow/batch/' . $row['batch_id']);
      unset($row['batch_id']);
    }
    // Now that the row has been adjusted to remove the "hidden" column that was used to create the link, make the column headings from the array_keys of the row.
    if (count($records_headings) < 1) {
      $records_headings = array('data'=>'Operations') + array_keys($row);
    }
    $rows[] = $row;
  }

  // Determine the field type for the fields on this table. 
  list($primary_key_field, $integer_fieldnames, $non_integer_fieldnames, $fields) = upitt_workflow_get_tablefield_stats($listname, $link_new);

  // Set the class for the heading cells based on field type.
  foreach ($fields as $field) {
    $headings_key = array_search($field, $records_headings);
    if (!(array_search($field, $integer_fieldnames) === FALSE)) {
      if (!($headings_key === FALSE)) {
        $records_headings[$headings_key] = array('data' => $field, 'class' => array('numeric'));
      }
    }
    if (!(array_search($field, $non_integer_fieldnames) === FALSE)) {
      if (!($headings_key === FALSE)) {
        $records_headings[$headings_key] = array('data' => $field, 'class' => array('non_numeric'));
      }
    }
  }

  // For each cell in the tables, add the class based on field type.
  if (count($integer_fieldnames) > 0) { 
    foreach ($integer_fieldnames as $integer_fieldname) {
      foreach ($rows as $key => $row) {
        $rows[$key][$integer_fieldname] = array('data' => $rows[$key][$integer_fieldname], 'class' => array('numeric'));
      }
    }
  }
  if (count($non_integer_fieldnames) > 0) {
    foreach ($non_integer_fieldnames as $non_integer_fieldname) {
      foreach ($rows as $key => $row) {
        $rows[$key][$non_integer_fieldname] = array('data' => $rows[$key][$non_integer_fieldname], 'class' => array('non_numeric'));
      }
    }
  }

  // Add a column for the operations on this row - using the primary key field.
  foreach ($rows as $key => $row) {
    $suffix = $rows[$key][$primary_key_field]['data'];
    $new_row = array('operations' => l('E', '/workflow/admin/manage/' . $listname . '/edit/' . $suffix, array('attributes' => array('title' => 'Edit'))) . ' | ' .
                                     l('X', '/workflow/admin/manage/' . $listname . '/delete/' . $suffix, array('attributes' => array('title' => 'Delete')) ));
    foreach ($row as $f => $v) {
      $new_row[$f] = $v;
    }
    $rows[$key] = $new_row;
  } 

  $form = array();

  mysqli_close($link_new);
  $pager = theme('pager', array('quantity',$count_rows));
  $table_markup = theme('upitt_workflow_batches',
      array('table' => $pager . theme('table', array('rows'=>$rows,'header'=>$records_headings,'attributes' =>array('class'=>array('report_table')))) . $pager,
            'links' => $links,
            'count_rows' => number_format($count_rows) . ' records' . ($listname ? ' in ' . $listname : ''),
            'breadcrumb' => $breadcrumb,
            'post_to' => '/workflow',
           )
    );
  $form['table_markup'] = array('#markup' => $table_markup);

  return $form;
}

/**
 * Handler to display form for edit any listname item.
 */
function upitt_workflow_admin_manage_edit($form, &$form_state, $listname = '', $primary_id = '') {
  // need to get the field and labels for the non-primary key fields
  $link_new = upitt_workflow_get_databaselink('mysql_new_workflow'); 

  list($primary_key_field, $integer_fieldnames, $non_integer_fieldnames, $fields) = upitt_workflow_get_tablefield_stats($listname, $link_new);
  $record = upitt_workflow_get_table_record($listname, $primary_key_field, $primary_id, $link_new);
  $all_fieldnames = implode(',', array_merge($integer_fieldnames, $non_integer_fieldnames));

  $links = upitt_workflow_get_listname_manage_links($listname);

  $form = array(
    'pk_fieldname' => array(
      '#type' => 'hidden',
      '#default_value' => $primary_key_field,
    ),
    'pk_value' => array(
      '#type' => 'hidden',
      '#default_value' => $primary_id,
    ),
    'table_update_fields' => array(
      '#type' => 'hidden',
      '#default_value' => $all_fieldnames,
    ),
    'table' => array(
      '#type' => 'hidden',
      '#default_value' => $listname,
    ),
  );

  foreach ($fields as $key => $field) {
    $form[$field] = array(
         '#type' => 'textfield',
         '#default_value' => $record[$field],
         '#title' => trim($field),
         '#attributes' => ''
      );
    if ($field == $primary_key_field) {
      $form[$field]['#attributes'] = array('class' => array('primary_key'), 'readonly' => 'readonly', 'disabled' => 'disabled');
      $form[$field]['#description'] = '`' . $primary_key_field . '` is the primary key for the `' . $listname . '` table.';
    }

  }

  // now add the Submit and Cancel buttons
  $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Save'),
    );
  $form['cancel'] = array(
      '#type' => 'submit',
      '#value' => t('Cancel'),
    );

  return $form;
}

/**
 * Handler to display form for edit any listname item.
 */
function upitt_workflow_admin_manage_edit_submit($form, &$form_state) {
  $table_listname = $form_state['values']['table'];
  $table_primary_id_fieldname = $form_state['values']['pk_fieldname'];
  $table_primary_id_value = $form_state['values']['pk_value'];
  $table_update_fields = $form_state['values']['table_update_fields'];
  $table_update_fields_array = explode(',', $table_update_fields);

  $btn_id = $form_state['triggering_element']['#id'];
  switch ($btn_id) {
    case 'edit-submit':
      $link_new = upitt_workflow_get_databaselink('mysql_new_workflow');
      $field_values = array();
      foreach ($table_update_fields_array as $field) {
        $field_values[$field] = $form_state['values'][$field];
      }
      $fields_to_values = array();
      foreach ($field_values as $field => $value) {
        $fields_to_values[] = '`' . $field . '` = \'' . mysqli_real_escape_string($link_new, $value) . '\'';
      }

      $sql = 'UPDATE ' . $table_listname . ' SET ' . implode(', ', $fields_to_values) . ' WHERE `' . $table_primary_id_fieldname . '` = \'' . $table_primary_id_value . '\'';
      if ($result = mysqli_query($link_new, $sql)) {
        drupal_set_message('Lookup table "' . $table_listname . '" updated for `' . $table_primary_id_fieldname . '` = "' . $table_primary_id_value . '"');
      }
      else {
        drupal_set_message('There was a problem updating the lookup table "' . $table_listname . '" for `' . $table_primary_id_fieldname . '` = "' . $table_primary_id_value . '"', 'error');
      }
      // save the record here.
      break;

    case 'edit-cancel':
      drupal_goto('/workflow/admin/manage/' . $table_listname);
      break;
  }
}

