// @ts-check

const eventToState = {
  COMMENT: 'COMMENTED',
  REQUEST_CHANGES: 'CHANGES_REQUESTED',
}

/**
 * @param {{
 *  github: InstanceType<import('@actions/github/lib/utils').GitHub>,
 *  context: import('@actions/github/lib/context').Context,
 *  core: import('@actions/core'),
 *  dry: boolean,
 *  reviewKey?: string,
 * }} DismissReviewsProps
 */
async function dismissReviews({ github, context, core, dry, reviewKey }) {
  const pull_number = context.payload.pull_request?.number
  if (!pull_number) {
    core.warning('dismissReviews called outside of pull_request context')
    return
  }

  if (dry) return

  const reviews = (
    await github.paginate(github.rest.pulls.listReviews, {
      ...context.repo,
      pull_number,
    })
  ).filter(
    (review) =>
      review.user?.login === 'github-actions[bot]' &&
      review.state !== 'DISMISSED',
  )
  const changesRequestedReviews = reviews.filter(
    (review) => review.state === 'CHANGES_REQUESTED',
  )

  const commentRegex = /<!-- projt review key: (.*)(?:; resolved: .*)? -->/
  const reviewKeyRegex = new RegExp(
    `<!-- (projt review key: ${reviewKey})(?:; resolved: .*)? -->`,
  )
  const commentResolvedRegex =
    /<!-- projt review key: .*; resolved: true -->/

  let reviewsToMinimize = reviews
  let /** @type {typeof reviews} */ reviewsToDismiss = []
  let /** @type {typeof reviews} */ reviewsToResolve = []

  if (reviewKey && reviews.every((review) => commentRegex.test(review.body))) {
    reviewsToMinimize = reviews.filter((review) =>
      reviewKeyRegex.test(review.body),
    )
  }

  if (
    changesRequestedReviews.every(
      (review) =>
        commentResolvedRegex.test(review.body) ||
        (reviewKey && reviewKeyRegex.test(review.body)),
    )
  ) {
    reviewsToDismiss = changesRequestedReviews
  } else if (reviewsToMinimize.length) {
    reviewsToResolve = reviewsToMinimize.filter(
      (review) =>
        review.state === 'CHANGES_REQUESTED' &&
        !commentResolvedRegex.test(review.body),
    )
  }

  await Promise.all([
    ...reviewsToMinimize.map(async (review) =>
      github.graphql(
        `mutation($node_id:ID!) {
              minimizeComment(input: {
                classifier: OUTDATED,
                subjectId: $node_id
              })
              { clientMutationId }
            }`,
        { node_id: review.node_id },
      ),
    ),
    ...reviewsToDismiss.map(async (review) =>
      github.rest.pulls.dismissReview({
        ...context.repo,
        pull_number,
        review_id: review.id,
        message: 'Review dismissed automatically',
      }),
    ),
    ...reviewsToResolve.map(async (review) =>
      github.rest.pulls.updateReview({
        ...context.repo,
        pull_number,
        review_id: review.id,
        body: review.body.replace(
          reviewKeyRegex,
          `<!-- projt review key: ${reviewKey}; resolved: true -->`,
        ),
      }),
    ),
  ])
}

/**
 * @param {{
 *  github: InstanceType<import('@actions/github/lib/utils').GitHub>,
 *  context: import('@actions/github/lib/context').Context
 *  core: import('@actions/core'),
 *  dry: boolean,
 *  body: string,
 *  event?: keyof eventToState,
 *  reviewKey: string,
 * }} PostReviewProps
 */
async function postReview({
  github,
  context,
  core,
  dry,
  body,
  event = 'REQUEST_CHANGES',
  reviewKey,
}) {
  const pull_number = context.payload.pull_request?.number
  if (!pull_number) {
    core.warning('postReview called outside of pull_request context')
    return
  }

  const reviewKeyRegex = new RegExp(
    `<!-- (projt review key: ${reviewKey})(?:; resolved: .*)? -->`,
  )
  const reviewKeyComment = `<!-- projt review key: ${reviewKey}; resolved: false -->`
  body = body + '\n\n' + reviewKeyComment

  const reviews = (
    await github.paginate(github.rest.pulls.listReviews, {
      ...context.repo,
      pull_number,
    })
  ).filter(
    (review) =>
      review.user?.login === 'github-actions[bot]' &&
      review.state !== 'DISMISSED',
  )

  /** @type {null | typeof reviews[number]} */
  let pendingReview
  const matchingReviews = reviews.filter((review) =>
    reviewKeyRegex.test(review.body),
  )

  if (matchingReviews.length === 0) {
    pendingReview = null
  } else if (
    matchingReviews.length === 1 &&
    matchingReviews[0].state === eventToState[event]
  ) {
    pendingReview = matchingReviews[0]
  } else {
    await dismissReviews({ github, context, core, dry, reviewKey })
    pendingReview = null
  }

  if (pendingReview !== null) {
    if (pendingReview.body === body) {
      core.info('Review is already up to date')
      return
    }

    core.info('Updating existing review')
    if (!dry) {
      await github.rest.pulls.updateReview({
        ...context.repo,
        pull_number,
        review_id: pendingReview.id,
        body,
      })
    }
  } else {
    core.info(`Posting review (event: ${event})`)
    if (!dry) {
      await github.rest.pulls.createReview({
        ...context.repo,
        pull_number,
        event,
        body,
      })
    }
  }
}

module.exports = { postReview, dismissReviews }
