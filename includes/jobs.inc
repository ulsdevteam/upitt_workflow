<?php

/**
 * This set of functions are registered as jobs that can be dispatched via 
 * islandora_job ~ using gearman to process.
 * 
 */

/**
 * A wrapper to help serialize objects for islandora_job.
 *
 * @see islandora_do_derivatives()
 */
function upitt_workflow_aggregate_ocr_pdf($object_in, array $options = array(), $entrances = 0) {
  return;
}

/**
 * A wrapper to help serialize objects for islandora_job.
 *
 * @see islandora_do_derivatives()
 */
function upitt_workflow_generate_ocr_datastreams($object_in, array $options = array(), $entrances = 0) {
  module_load_include('inc', 'islandora', 'includes/derivatives');
  module_load_include('inc', 'islandora', 'includes/utilities');
  module_load_include('inc', 'islandora_job_jobs', 'includes/utilities');

  // XXX: $object_in at one point was a serialized AbstractObject. Let's get its
  // string value (which should be its PID), to load it properly, to avoid any
  // potential issues which come from using stale objects.
  $object_id = islandora_is_valid_pid($object_in) ?
    $object_in :
    strval(unserialize($object_in));
  // XXX: Due to how Drush bootstrapping works, the connection may be created
  // without credentials (when your site's front page is
  // 'islandora/object/some:object', for example). Resetting to ensure a new
  // connection gets created should fix it.
  drupal_static_reset('islandora_get_tuque_connection');
  $object = islandora_object_load($object_id);
  if (!is_object($object)) {
    return;
  }

  module_load_include('inc', 'islandora_job', 'includes/utilities');
  $payload = islandora_job_format_payload('upitt_workflow_generate_ocr_datastreams', func_get_args());

  if (!islandora_job_jobs_enforce_idle_wait($object, $entrances)) {
    islandora_job_submit_background('upitt_workflow_generate_ocr_datastreams', $object->id, $options, $entrances + 1);
    return;
  }

  module_invoke_all('islandora_job_started', $payload);

//BGG - no need for defer derivatives  $re_add = islandora_get_defer_derivatives_flag($object);
//BGG - no need for defer derivatives  islandora_remove_defer_derivatives_flag($object);

  // The $options array here could potentially have "aggregate_ocr", but handling this
  // is slightly complex because the status of ALL pages needs to be completed before
  // aggregation can take place.  That aggregation step is a completely separate "job".
  $result = upitt_workflow_do_ocr($object, $options);

  // If a derivative callback returns a batch, it needs to be forced to process
  // as there's no form submits or existing batches to trigger it.
  $batch = &batch_get();
  if (!empty($batch)) {
    drush_backend_batch_process();
  }

  return $result;
}


/**
 * This is not actually a registered jobs function, but a helper function used
 * by `upitt_workflow_generate_ocr_datastreams` above.
 *
 * @param type $islandora_object
 * @param type $options
 * @return boolean
 */
function upitt_workflow_do_ocr(AbstractObject $islandora_object, $options) {
  // PROBABLY should not do this twice::      $batch =& batch_get();

  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  module_load_include('inc', 'islandora_paged_content', 'includes/batch');
  module_load_include('inc', 'islandora_upitt', 'includes/drush_helpers');
  $time_start = microtime_float();

  if (!is_object($islandora_object) || (get_class($islandora_object) <> 'IslandoraFedoraObject')){
    drush_set_error(dt('Object provided is not an islandora object.'));
    return false;
  }
  else {
    global $user;
    global $base_url;
    $args = func_get_args();

    array_shift($args);
    // to ensure that pid is first arg passed as payload
    array_unshift($args, $islandora_object->id);
    $payload = json_encode(
      array(
        "func" => 'upitt_workflow_generate_ocr_datastreams',
        "args" => $args,
        "site" => $base_url,
        "uid" => $user->uid,
      )
    );

    $payload = array('object_id' => $islandora_object->id);
    module_invoke_all('islandora_job_started', $payload);

    $aggregate_ocr = drush_get_option('aggregate_ocr', 0);
    $pages = array_keys(islandora_paged_content_get_pages($islandora_object));
    $page_count = count($pages);
    $options = array(
      'language' => 'eng',
      'preprocess' => FALSE,
      'aggregate_ocr' => $aggregate_ocr,
    );
    $batch = islandora_paged_content_create_ocr_batch($islandora_object, $pages, $options);

    batch_set($batch);
    $batch =& batch_get();

    //Because we are doing this on the back-end, we set progressive to false.
    $batch['progressive'] = FALSE;

    drush_backend_batch_process();
    $results = array_keys($pages);
  }
  $time_end = microtime_float();
  $time = $time_end - $time_start;
  drush_print(number_format($page_count) . ' pages processed in ' . $time);
  if ($page_count > 0) {
    drush_print($time / $page_count . ' seconds per page');
  }
  return $results;
}

