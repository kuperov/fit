
read.fit <- function(filename) {
  fit_message_tables <- decode_fit_file(filename);
  for (table in names(fit_message_tables)) {
    unitattr <- attr(fit_message_tables[[table]], 'units')
    tbl <- as.data.frame(fit_message_tables[[table]])
    if (!is.null(unitattr)) {
      # HACK - incantation to convert 'semicircles' to degrees
      #        this should be done in C++ when the data type is available
      sc_cols = which(unitattr == 'semicircles')
      tbl[,sc_cols] <- (tbl[,sc_cols] * 180 / 2^31 + 180) %% 360 - 180
      unitattr[sc_cols] <- 'degrees'
      attr(tbl, 'units') <- unitattr
    }
    fit_message_tables[[table]] <- tbl
  }
  fit_message_tables
}
