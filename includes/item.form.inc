<?php 

/**
 * When no value is passed in for do_id, this could be used as a form to for create items.
 */
function upitt_workflow_item_edit(array $form, array &$form_state, $do_id = NULL) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  
  $sites = upitt_workflow_get_mysql_options('wflocal_fedora_site', 'Site', 'pid', 'name', 'name', 0);
  $types = upitt_workflow_get_mysql_options('item_type', 'Type', 'id', 'description', 'description', 0);
  
  // Special handling for these `type` table descriptons ---- they are so long, we must make the text shorter to display in a select box.
  foreach ($types as $k => $type) {
    $types[$k] = strlen($type) > 50 ? substr($type,0,50)."..." : $type;
  }

  // for legacy batches, this array may need to be joined with the legacy collection `c_id` pointers instead of Solr PID values.
  $collections = upitt_workflow_get_solr_options('RELS_EXT_hasModel_uri_ms:info\:fedora\/islandora\:collectionCModel', 'PID', 'fgs_label_s');
  asort($collections);
  $item = upitt_workflow_get_item($do_id);

  $read_only_att = array('readonly' => 'readonly', 'disabled' => 'disabled');

  $form = array(  
    'id'=>array(
      '#type' => 'hidden',
      '#default_value' => $item['id'],
    ),
    'do_id'=>array(
      '#type' => 'textfield',
      '#title' => t('do_id'),
      '#attributes' => $read_only_att,
      '#description' => t('This is the object\'s object_id.'),
      '#default_value' => $item['do_id'],
    ),
    'name'=>array(
      '#type' => 'textfield',
      '#title' => t('Name'),
      '#default_value' => $item['name'],
    ),
    'type_id'=>array(
      '#type' => 'select',
      '#title' => t('Type'),
      '#options' => $types,
      '#default_value' => ($item && isset($item['type_id']))? $item['type_id'] : 0,
    ),
    'property_owner_id'=>array(
      '#type' => 'textfield',
      '#title' => t('Property Owner ID'),
      '#default_value' => $item['property_owner_id'],
    ),

    'collections' => array(
      '#type' => 'select',
      '#description' => 'Hold down Ctrl to select multiple collections',
      '#title' => t('Collection'),
      '#multiple' => TRUE,
      '#options' => $collections,
      '#default_value' => ($item && isset($item['collections']))? explode(",", $item['collections']) : '',
      '#size' => 10,
    ),
    'sites' => array(
      '#type' => 'select',
      '#description' => 'Hold down Ctrl to select multiple sites',
      '#title' => t('Site'),
      '#multiple' => TRUE,
      '#options' => $sites,
      '#default_value' => ($item && isset($item['sites']))? explode(",", $item['sites']) : '',
      '#size' => 10,
    ),
    'submit' => array(
      '#type' => 'submit',
      '#value' => t('Save batch'),
    ),
  );  
  return $form;
}

function upitt_workflow_item_edit_submit(array $form, array &$form_state) {
  global $user;

  $object_id = upitt_workflow_save_item($form_state['values']);
  $object_id = $form_state['values']['do_id'];
  drupal_set_message('Updated "' . $object_id . '".');

  $form_state['redirect'] = '/workflow/object/' . $object_id;
}

/**
 * This will save or update the given item record.
 */
function upitt_workflow_save_item($values) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  $pk_item_id = $values['id'];
  $collections_values = $values['collections'];
  $sites_values = $values['sites'];

  $object_id = $values['do_id'];

  $link = upitt_workflow_get_databaselink('mysql_new_workflow');

  $sql_fields = $sql_values = array();
  $lookup_fields = array('collections', 'sites', 'type_id');
  $item_fields = array('id', 'do_id', 'name', 'property_owner_id');

  // assume all fields need to be escaped and wrapped with '' characters -- EXCEPT for the integer fields.
  foreach ($values as $fieldname => $value) {
    if (!(array_search($fieldname, $lookup_fields) === FALSE)) {  
      // need to populate the lookup array of sites or collections values for this object
    }
    else {
      if (!(array_search($fieldname, $item_fields) === FALSE)) {
        $sql_fields[] = $fieldname;

        if (is_array($value)) {
          $value = implode(",", $value);
         }
        $sql_values[] = "'" . mysqli_real_escape_string($link, $value) . "'";
      }
    }
  }
  
  $sql = "REPLACE INTO `item` (`" . implode("`, `", $sql_fields) . "`) VALUES (" . implode(", ", $sql_values) . ")";

  $result = mysqli_query($link, $sql);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $sql);
  }

  mysqli_close($link);

  // Now, update the lookup tables for item-collections and item-sites
  upitt_workflow_save_item_collections($pk_item_id, $collections_values);
  upitt_workflow_save_item_sites($pk_item_id, $sites_values);

  return $object_id;
}

function upitt_workflow_save_item_collections($pk_item_id, $collections) {
  if (count($collections) > 0) {
    $link = upitt_workflow_get_databaselink('mysql_new_workflow');
    // Remove any existing collections records for this object before inserting the new relationships.
    $sql = 'DELETE FROM `wflocal_local_item_fedora_collections` WHERE `local_item_id` = ' . $pk_item_id;
    $result = mysqli_query($link, $sql);
    if (!$result) { upitt_workflow_sql_error_die($link, $sql); }

    $sql = array();
    foreach ($collections as $collection_pid) {
      $sql = 'INSERT INTO `wflocal_local_item_fedora_collections` (`local_item_id`, `fedora_collection_id`) VALUES ' . 
             '(' . $pk_item_id . ', ' . 
             ' (SELECT c.id FROM `wflocal_fedora_collection` c WHERE c.pid = \'' . upitt_workflow_safe_qstring($link, $collection_pid) . '\')' .
             ')';
      $result = mysqli_query($link, $sql);
      if (!$result) { upitt_workflow_sql_error_die($link, $sql); }
    }
    mysqli_close($link);
  }
}

function upitt_workflow_save_item_sites($pk_item_id, $sites) {
  if (count($sites) > 0) {
    $link = upitt_workflow_get_databaselink('mysql_new_workflow');
    // Remove any existing sites records for this object before inserting the new relationships.
    $sql = 'DELETE FROM `wflocal_local_item_fedora_sites` WHERE `local_item_id` = ' . $pk_item_id;
    $result = mysqli_query($link, $sql);
    if (!$result) { upitt_workflow_sql_error_die($link, $sql); }

    $sql = array();
    foreach ($sites as $site_pid) {
      $sql = 'INSERT INTO `wflocal_local_item_fedora_sites` (`local_item_id`, `fedora_site_id`) VALUES ' .
             '(' . $pk_item_id . ', ' .
             ' (SELECT s.id FROM `wflocal_fedora_site` s WHERE s.pid = \'' . upitt_workflow_safe_qstring($link, $site_pid) . '\')' .
             ')';
      $result = mysqli_query($link, $sql);
      if (!$result) { upitt_workflow_sql_error_die($link, $sql); }
    }
    mysqli_close($link);
  }
}
