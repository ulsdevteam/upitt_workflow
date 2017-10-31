<?php

/**
 * Helper function to create the search form filter choices for collections in workflow_django
 */
function upitt_workflow_add_barcode_to_hopper($pid, $link) {
  module_load_include('inc', 'upitt_workflow', 'includes/process_barcodes.form');
  @list($namespace, $barcode) = explode(":", $pid);
  // This will return the size of the files that would be copied to the
  // object's /ingest/tmp batch folder.
  $checksize_result = upitt_workflow_do_copy_for_barcodes($barcode, TRUE, FALSE);
  @list($junk, $size) = explode("=", trim($checksize_result));
  $existed = upitt_workflow_get_hopper_for_pid($pid, FALSE, $link);
  if (!$existed) {

    $size =
    $query = "INSERT INTO `ingest_hopper_queue` (`PID`, `files_size`) VALUES ('" . mysqli_real_escape_string($link, $pid) . "', " . $size . ")";
    $result = mysqli_query($link, $query);
    if (!$result) {
      upitt_workflow_sql_error_die($link, $query);
    }
    return TRUE;
  }
  else {
    return FALSE;
  }
}

function upitt_workflow_get_pending_hopper_queue_items($link) {
  $query = "SELECT `PID`, `files_size`, `weight` FROM `ingest_hopper_queue` WHERE ingest_start_timestamp IS NULL ORDER BY `weight`";
  $result = mysqli_query($link, $query);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $query);
  }
  $rows = array();
  while ($row = mysqli_fetch_assoc($result)) {
    $rows[] = $row;
  }
  return $rows;
}

function upitt_workflow_get_hopper_for_pid($pid, $pending_objects_only, $link) {
  $query = "SELECT * FROM `ingest_hopper_queue` WHERE `PID` = '" . mysqli_real_escape_string($link, $pid) . "'";
  if ($pending_objects_only) {
    $query .= " AND ingest_start_timestamp IS NULL";
  }
  $result = mysqli_query($link, $query);
  if (!$result) {
    upitt_workflow_sql_error_die($link, $query);
  }
  $row = mysqli_fetch_assoc($result);
  if (isset($row['PID'])) {
    return $row;
  }
  else {
    return FALSE;
  }
}

// upitt_workflow_hopper_increase_priority
function upitt_workflow_hopper_increase_priority($pid, $link) {
  $existed = upitt_workflow_get_hopper_for_pid($pid, FALSE, $link);
  if ($existed) {
    $query = "UPDATE `ingest_hopper_queue` SET weight = weight - 5 WHERE `PID` = '" . mysqli_real_escape_string($link, $pid) . "'";
    $result = mysqli_query($link, $query);
    if (!$result) {
      upitt_workflow_sql_error_die($link, $query);
    }
    // now call hopper_list_pending to display the weight "priority" values for these.
    $hopper_row = upitt_workflow_get_hopper_for_pid($pid, FALSE, $link);

    if (is_array($hopper_row)) {
      $markup = 'increased for ' . $pid . '<br>' . $hopper_row['weight'] . '<br>';
    }
    else {
      $markup = '<span class="bad">hopper record not found for PID ' . $pid . '</span><br>';
    }
    return $markup;
  }
  else {
    return FALSE;
  }
}

function upitt_workflow_hopper_decrease_priority($pid, $link) {
  $existed = upitt_workflow_get_hopper_for_pid($pid, FALSE, $link);
  if ($existed) {
    $query = "UPDATE `ingest_hopper_queue` SET weight = weight + 5 WHERE `PID` = '" . mysqli_real_escape_string($link, $pid) . "'";
    $result = mysqli_query($link, $query);
    if (!$result) {
      upitt_workflow_sql_error_die($link, $query);
    }
    // now call hopper_list_pending to display the weight "priority" values for these.
    $hopper_row = upitt_workflow_get_hopper_for_pid($pid, FALSE, $link);

    if (is_array($hopper_row)) {
      $markup = 'decreased for ' . $pid . '<br>' . $hopper_row['weight'] . '<br>';
    }
    else {
      $markup = '<span class="bad">hopper record not found for PID ' . $pid . '</span><br>';
    }
    return $markup;
  }
  else {
    return FALSE;
  }
}

