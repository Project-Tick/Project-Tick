use nom::IResult;
use nom::Parser;
use nom::branch::alt;
use nom::bytes::complete::tag;
use nom::bytes::complete::tag_no_case;
use nom::character::complete::multispace0;
use nom::character::complete::multispace1;
use nom::combinator::map;
use nom::multi::many1;
use nom::sequence::preceded;
use tracing::warn;

pub fn parse(text: &str) -> Option<Vec<Instruction>> {
    let instructions: Vec<Instruction> = text
        .lines()
        .flat_map(|s| match parse_line(s) {
            Some(instructions) => instructions.into_iter(),
            None => Vec::new().into_iter(),
        })
        .collect();

    if instructions.is_empty() {
        None
    } else {
        Some(instructions)
    }
}

fn is_not_whitespace(c: char) -> bool {
    !c.is_ascii_whitespace()
}

fn normal_token(input: &str) -> IResult<&str, String> {
    let (input, tokens) =
        many1(nom::character::complete::satisfy(is_not_whitespace)).parse(input)?;

    let s: String = tokens.into_iter().collect();
    if s.eq_ignore_ascii_case("@tickbot") {
        Err(nom::Err::Error(nom::error::Error::new(
            input,
            nom::error::ErrorKind::Tag,
        )))
    } else {
        Ok((input, s))
    }
}

fn parse_command(input: &str) -> IResult<&str, Instruction> {
    alt((
        preceded(
            preceded(multispace0, tag("build")),
            preceded(
                multispace1,
                map(many1(preceded(multispace0, normal_token)), |targets| {
                    Instruction::Build(Subset::Project, targets)
                }),
            ),
        ),
        preceded(
            preceded(multispace0, tag("test")),
            preceded(
                multispace1,
                map(many1(preceded(multispace0, normal_token)), |targets| {
                    Instruction::Test(targets)
                }),
            ),
        ),
        preceded(multispace0, map(tag("eval"), |_| Instruction::Eval)),
    ))
    .parse(input)
}

fn parse_line_impl(input: &str) -> IResult<&str, Option<Vec<Instruction>>> {
    let (input, _) = multispace0.parse(input)?;

    let result = map(
        many1(preceded(
            multispace0,
            preceded(
                tag_no_case("@tickbot"),
                preceded(multispace1, parse_command),
            ),
        )),
        |instructions| Some(instructions),
    )
    .parse(input);

    match result {
        Ok((rest, instructions)) => Ok((rest, instructions)),
        Err(_e) => Ok((input, None)),
    }
}

pub fn parse_line(text: &str) -> Option<Vec<Instruction>> {
    match parse_line_impl(text) {
        Ok((_, res)) => res,
        Err(e) => {
            warn!("Failed parsing string '{}': result was {:?}", text, e);
            None
        }
    }
}

#[derive(PartialEq, Eq, Debug, Clone)]
pub enum Instruction {
    Build(Subset, Vec<String>),
    Test(Vec<String>),
    Eval,
}

#[allow(clippy::upper_case_acronyms)]
#[derive(serde::Serialize, serde::Deserialize, Debug, PartialEq, Eq, Clone)]
pub enum Subset {
    Project,
}

#[cfg(test)]
mod tests {

    use super::*;

    #[test]
    fn parse_empty() {
        assert_eq!(None, parse(""));
    }

    #[test]
    fn valid_trailing_instruction() {
        assert_eq!(
            Some(vec![Instruction::Eval]),
            parse(
                "/cc @samet for ^^
@tickbot eval",
            )
        );
    }

    #[test]
    fn bogus_comment() {
        assert_eq!(None, parse(":) :) :) @tickbot build hi"));
    }

    #[test]
    fn bogus_build_comment_empty_list() {
        assert_eq!(None, parse("@tickbot build"));
    }

    #[test]
    fn eval_comment() {
        assert_eq!(Some(vec![Instruction::Eval]), parse("@tickbot eval"));
    }

    #[test]
    fn eval_and_build_comment() {
        assert_eq!(
            Some(vec![
                Instruction::Eval,
                Instruction::Build(Subset::Project, vec![String::from("meshmc")]),
            ]),
            parse("@tickbot eval @tickbot build meshmc")
        );
    }

    #[test]
    fn build_and_eval_and_build_comment() {
        assert_eq!(
            Some(vec![
                Instruction::Build(Subset::Project, vec![String::from("mnv")]),
                Instruction::Eval,
                Instruction::Build(Subset::Project, vec![String::from("meshmc")]),
            ]),
            parse(
                "
@tickbot build mnv
@tickbot eval
@tickbot build meshmc",
            )
        );
    }

    #[test]
    fn complex_comment_with_paragraphs() {
        assert_eq!(
            Some(vec![
                Instruction::Build(Subset::Project, vec![String::from("mnv")]),
                Instruction::Eval,
                Instruction::Build(Subset::Project, vec![String::from("meshmc")]),
            ]),
            parse(
                "
I like where you're going with this PR, so let's try it out!

@tickbot build mnv

I noticed though that the target branch was broken, which should be fixed. Let's eval again.

@tickbot eval

Also, just in case, let's try meshmc
@tickbot build meshmc",
            )
        );
    }

    #[test]
    fn build_and_eval_comment() {
        assert_eq!(
            Some(vec![
                Instruction::Build(Subset::Project, vec![String::from("meshmc")]),
                Instruction::Eval,
            ]),
            parse("@tickbot build meshmc @tickbot eval")
        );
    }

    #[test]
    fn build_comment() {
        assert_eq!(
            Some(vec![Instruction::Build(
                Subset::Project,
                vec![String::from("meshmc"), String::from("mnv")]
            ),]),
            parse(
                "@tickbot build meshmc mnv

neozip",
            )
        );
    }

    #[test]
    fn test_comment() {
        assert_eq!(
            Some(vec![Instruction::Test(
                vec![
                    String::from("meshmc"),
                    String::from("mnv"),
                    String::from("neozip"),
                ]
            ),]),
            parse("@tickbot test meshmc mnv neozip")
        );
    }

    #[test]
    fn build_comment_newlines() {
        assert_eq!(
            Some(vec![Instruction::Build(
                Subset::Project,
                vec![
                    String::from("meshmc"),
                    String::from("mnv"),
                    String::from("neozip"),
                ]
            ),]),
            parse("@tickbot build meshmc mnv neozip")
        );
    }

    #[test]
    fn build_comment_case_insensitive_tag() {
        assert_eq!(
            Some(vec![Instruction::Build(
                Subset::Project,
                vec![
                    String::from("meshmc"),
                    String::from("mnv"),
                    String::from("neozip"),
                ]
            ),]),
            parse("@TickBot build meshmc mnv neozip")
        );
    }

    #[test]
    fn build_comment_lower_package_case_retained() {
        assert_eq!(
            Some(vec![Instruction::Build(
                Subset::Project,
                vec![
                    String::from("meshmc"),
                    String::from("mnv"),
                    String::from("json4cpp"),
                ]
            ),]),
            parse("@tickbot build meshmc mnv json4cpp")
        );
    }
}
