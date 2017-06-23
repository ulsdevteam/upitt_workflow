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
  module_load_include('inc', 'upitt_workflow', 'includes/utilities');

  $object_id = $form_state['values']['do_id'];
  $ingest_namespace = upitt_workflow_get_ingest_namespace();
  $sites = $form_state['values']['sites'];
  $collections = $form_state['values']['collections'];

  $sites_arr = explode(",", $sites);
  $collections_arr = explode(",", $collections);
  
  if (count($sites_arr) > 0 || count($collections_arr) > 0) {
    // 1. load the islandora_object - and loop through collections and sites to add
    // these relationships using the upitt_workflow_addRelationship - which will ensure
    // that duplicate relationships are not added.
    $pid = $ingest_namespace . $object_id;
    $object = islandora_object_load($pid);

    foreach ($sites_arr as $site) {
      $object->relationships->remove('http://digital.library.pitt.edu/ontology/relations#', 'isMemberOfSite', $site);
      $object->relationships->add('http://digital.library.pitt.edu/ontology/relations#', 'isMemberOfSite', $site);
    }

    foreach ($collections_arr as $collection) {
      $object->relationships->remove(FEDORA_RELS_EXT_URI, 'isMemberOfCollection', $collection);
      $object->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOfCollection', $collection);
    }

    upitt_workflow_set_status_for_batch($object_id, UPITT_WORKFLOW_ACTION_OBJECT_PUBLISHED_ON_SITE, NULL);
    drupal_set_message('Published "' . $object_id . '" to sites and collections.');
  }
  else {
    drupal_set_message('There were no sites or collections related to the object "' . $object_id . '"', 'warning');
  }
  $form_state['redirect'] = '/workflow/object/' . $object_id;
}

