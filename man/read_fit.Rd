\name{read.fit}
\alias{read.fit}
\docType{package}
\title{
Read a FIT file into R
}
\description{
Decodes a FIT file, producing a single table for each message type.
}
\usage{
fit <- read.fit(filename)
}
\examples{
\dontrun{
gps <- read.fit('workout.fit')$record
plot(density(gps$heart_rate), main="Heart rate density")
}
}
