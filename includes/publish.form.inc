<?php 

/**
 * When no value is passed in for do_id, this could be used as a form to for create items.
 */
function upitt_workflow_do_publish(array $form, array &$form_state, $do_id = NULL) {
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');
  
  $site_options = upitt_workflow_get_mysql_options('wflocal_fedora_site', 'Site', 'pid', 'name', 'name', 0);
  
  // for legacy batches, this array may need to be joined with the legacy collection `c_id` pointers instead of Solr PID values.
  $collection_options = upitt_workflow_get_solr_options('RELS_EXT_hasModel_uri_ms:info\:fedora\/islandora\:collectionCModel', 'PID', 'fgs_label_s');
  asort($collection_options);
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
    'collections' => array(
      '#type' => 'select',
      '#description' => 'Hold down Ctrl to select multiple collections',
      '#title' => t('Collection'),
      '#multiple' => TRUE,
      '#options' => $collection_options,
      '#default_value' => ($item && isset($item['collections']))? explode(",", $item['collections']) : '',
      '#size' => 10,
    ),
    'sites' => array(
      '#type' => 'select',
      '#description' => 'Hold down Ctrl to select multiple sites',
      '#title' => t('Site'),
      '#multiple' => TRUE,
      '#options' => $site_options,
      '#default_value' => ($item && isset($item['sites']))? explode(",", $item['sites']) : '',
      '#size' => 10,
    ),
    'submit' => array(
      '#type' => 'submit',
      '#value' => t('Publish'),
    ),
  );  
  return $form;
}

function upitt_workflow_do_publish_submit(array $form, array &$form_state) {
  global $user;

  $object_id = upitt_workflow_save_item($form_state['values']);
  upitt_workflow_set_status_for_batch($object_id, UPITT_WORKFLOW_ACTION_OBJECT_PUBLISHED_ON_SITE, NULL);

  drupal_set_message('Published "' . $object_id . '" to sites and collections.');

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

  return $object_id;
}
